#include "singleinstance.h"

#include <QCryptographicHash>
#include <QDir>
#include <QElapsedTimer>
#include <QLocalServer>
#include <QLocalSocket>
#include <QLockFile>
#include <QThread>

namespace {

// A request is a few short strings and the connection carries nothing else, so
// the frame is the connection: the sender writes and closes, and the receiver
// takes what arrived.  Newline separates the parts, and a config path cannot
// contain one on any platform this builds for.
const char kSeparator = '\n';

// Long enough that a busy machine does not give up on a program that is there,
// short enough that a click does not appear to do nothing.
const int kTimeoutMs = 2000;

// Long enough for the lock to be tried again once a dead instance's file has
// been cleared away, which is what this wait is really for.
const int kLockWaitMs = 200;

}  // namespace

QString SingleInstance::keyForConfigDir(const QString &dir)
{
    // Hashed rather than spelled out because a unix socket path has to fit in
    // sockaddr_un -- about a hundred characters, which a config directory
    // under a long home directory can eat on its own.  Sixteen hex digits of
    // SHA-1 is far more than enough to tell two directories apart.
    const QByteArray digest =
        QCryptographicHash::hash(dir.toUtf8(), QCryptographicHash::Sha1);
    return QStringLiteral("vclock-") +
           QString::fromLatin1(digest.toHex().left(16));
}

SingleInstance::SingleInstance(const QString &key, QObject *parent)
    : QObject(parent), m_name(key)
{
    // A lock file decides which instance is the one that runs; the socket only
    // carries messages to it.  The socket cannot decide, because QLocalServer
    // on Unix unlinks whatever it finds at the name and binds a fresh socket
    // there: listening always succeeds, so a second instance would quietly
    // take the name away from the first and leave it running and unreachable.
    // QLockFile is the other way round -- it is a lock and nothing else, it
    // records the pid holding it, and it is the same mechanism on Windows.
    m_lock = new QLockFile(QDir::tempPath() + QLatin1Char('/') + key +
                           QStringLiteral(".lock"));
    // Without this a lock counts as stale once it is half a minute old, which
    // for a clock that runs all day would be always.  At zero the only thing
    // that makes a lock stale is the process that took it being gone, which is
    // the question actually being asked.
    m_lock->setStaleLockTime(0);
    m_primary = m_lock->tryLock(kLockWaitMs);

    if (!m_primary)
        return;

    m_server = new QLocalServer(this);
    // Otherwise the socket is created against the umask, and on a multi-user
    // machine another account could talk to this one's clocks.
    m_server->setSocketOptions(QLocalServer::UserAccessOption);
    m_server->listen(m_name);
    connect(m_server, &QLocalServer::newConnection, this, &SingleInstance::accept);
}

SingleInstance::~SingleInstance()
{
    delete m_lock;
}

void SingleInstance::accept()
{
    while (QLocalSocket *socket = m_server->nextPendingConnection()) {
        auto *buffer = new QByteArray;
        connect(socket, &QLocalSocket::readyRead, socket,
                [socket, buffer] { buffer->append(socket->readAll()); });
        connect(socket, &QLocalSocket::disconnected, this, [this, socket, buffer] {
            buffer->append(socket->readAll());
            const QString text = QString::fromUtf8(*buffer);
            delete buffer;
            socket->deleteLater();
            if (!text.isEmpty())
                emit received(text.split(QLatin1Char(kSeparator)));
        });
    }
}

bool SingleInstance::send(const QStringList &request)
{
    QLocalSocket socket;
    // The instance holding the lock may have taken it a moment ago and not yet
    // reached listen(), and connecting to a socket that is not there fails at
    // once rather than waiting.  So the waiting is done here, around the
    // attempt, instead of inside it.
    QElapsedTimer elapsed;
    elapsed.start();
    for (;;) {
        socket.connectToServer(m_name);
        if (socket.waitForConnected(kTimeoutMs))
            break;
        if (elapsed.elapsed() >= kTimeoutMs)
            return false;
        QThread::msleep(20);
    }

    socket.write(request.join(QLatin1Char(kSeparator)).toUtf8());
    if (!socket.waitForBytesWritten(kTimeoutMs))
        return false;

    // The read end sees the request when the write end closes, so the close has
    // to happen here rather than being left to the destructor, which would run
    // after the process had already decided it had succeeded.
    socket.disconnectFromServer();
    if (socket.state() != QLocalSocket::UnconnectedState)
        socket.waitForDisconnected(kTimeoutMs);
    return true;
}
