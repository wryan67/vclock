// Making a second launch reach the clock that is already running.
//
// Until now every launch was its own program, which was fine while the only
// way to start one was to type its name.  A pinned taskbar button is not that:
// clicking it, or picking Manage clocks off its right-click menu, is how a
// Windows user expects to get *back* to a program that is already up.  Without
// somewhere for that click to go it would start a second copy, and two copies
// sharing one config directory would each save over the other.
//
// So the first instance listens on a local socket and every later one hands
// over what it was asked to do and stops.  QLocalServer is a named pipe on
// Windows and a unix socket elsewhere, which is the whole of the platform
// difference.
#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

class QLocalServer;
class QLockFile;

class SingleInstance : public QObject
{
    Q_OBJECT

public:
    // `key` names the socket.  Instances with different keys do not see each
    // other, which is what config directory below relies on.
    explicit SingleInstance(const QString &key, QObject *parent = nullptr);
    ~SingleInstance() override;

    // True when nothing was already listening, so this process is the one that
    // runs the clocks.  False means send() should be called instead.
    bool isPrimary() const { return m_primary; }

    // Hand a request to the running instance.  False if it could not be
    // delivered, which can only happen if that instance stopped between the
    // constructor and here; the caller then carries on under its own steam.
    bool send(const QStringList &request);

    // What the socket is called for a given config directory.  Two instances
    // reading different configs are different programs and must not be folded
    // into one -- that is what makes it safe to run a test copy with
    // XDG_CONFIG_HOME pointed somewhere else while the real one is up.
    static QString keyForConfigDir(const QString &dir);

signals:
    // A later launch asked for something.  The list is what was passed to
    // send().
    void received(const QStringList &request);

private:
    void accept();

    QLocalServer *m_server = nullptr;
    QLockFile *m_lock = nullptr;
    QString m_name;
    bool m_primary = false;
};
