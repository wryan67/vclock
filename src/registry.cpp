#include "registry.h"

#include "config.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSet>

namespace {

ClockEntry defaultEntry()
{
    ClockEntry entry;
    entry.file = defaultClockFile();
    entry.name = QStringLiteral("Default");
    entry.show = true;
    return entry;
}

// Config files already sitting in the config directory, so an install that
// predates the registry finds its clocks listed the first time it starts.
Registry discoverExisting()
{
    Registry registry;
    registry.clocks.push_back(defaultEntry());

    QDir dir(configDir());
    const QStringList files =
        dir.entryList({QStringLiteral("*.cfg")}, QDir::Files, QDir::Name);
    for (const QString &file : files) {
        // default.cfg is already in; vclocks.cfg is this file; vclock.cfg is
        // the config an older release wrote, which default.cfg was migrated
        // from.  clocks.cfg is only still here if the move off the old
        // registry name failed, so skip it rather than read a list as a clock.
        if (file == defaultClockFile() || file == QLatin1String("vclocks.cfg")
            || file == QLatin1String("clocks.cfg") || file == QLatin1String("vclock.cfg"))
            continue;
        ClockEntry entry;
        entry.file = file;
        entry.name = QFileInfo(file).completeBaseName();
        entry.show = false;
        registry.clocks.push_back(entry);
    }
    return registry;
}

}  // namespace

QString ClockEntry::path() const
{
    if (file.isEmpty())
        return configPath();
    if (QDir::isAbsolutePath(file))
        return file;
    return QDir(configDir()).filePath(file);
}

int Registry::indexOfFile(const QString &file) const
{
    for (int i = 0; i < clocks.size(); ++i) {
        if (clocks.at(i).file == file)
            return i;
    }
    return -1;
}

int Registry::indexOfPath(const QString &path) const
{
    const QFileInfo wanted(path);
    for (int i = 0; i < clocks.size(); ++i) {
        if (QFileInfo(clocks.at(i).path()) == wanted)
            return i;
    }
    return -1;
}

const ClockEntry *Registry::findPath(const QString &path) const
{
    const int index = indexOfPath(path);
    return index < 0 ? nullptr : &clocks.at(index);
}

QString defaultClockFile()
{
    return QStringLiteral("default.cfg");
}

bool isDefaultClockFile(const QString &file)
{
    if (file.isEmpty())
        return false;
    if (QDir::isAbsolutePath(file))
        return QFileInfo(file) == QFileInfo(configPath());
    return file == defaultClockFile();
}

QString clockFileName(const QString &name)
{
    return name + QStringLiteral(".cfg");
}

QString cleanClockName(const QString &name)
{
    QString clean = name.trimmed();
    if (clean.size() > 4 && clean.endsWith(QLatin1String(".cfg"), Qt::CaseInsensitive))
        clean.chop(4);
    return clean.trimmed();
}

QString clockNameError(const QString &name, const Registry &registry, const QString &exceptFile)
{
    const QString clean = cleanClockName(name);
    if (clean.isEmpty())
        return QStringLiteral("A clock needs a name.");

    // The rules are the strictest of the platforms vclock runs on rather than
    // the ones this platform happens to enforce.  A config directory is a
    // thing people carry between machines, and a name that is fine on the one
    // it was typed on is no use if it cannot be written on the next.
    for (const QChar c : clean) {
        if (c.unicode() < 32)
            return QStringLiteral("A clock's name cannot contain control characters.");
        if (QStringLiteral(R"(/\:*?"<>|)").contains(c)) {
            return QStringLiteral("A clock's name is its file name too, so it cannot contain %1. "
                                  "None of \\ / : * ? \" < > | may be used.")
                .arg(c);
        }
    }
    if (clean.startsWith(QLatin1Char('.')))
        return QStringLiteral("A clock's name cannot begin with a dot.");
    if (clean.endsWith(QLatin1Char('.')))
        return QStringLiteral("A clock's name cannot end with a dot.");

    // Names Windows keeps for devices.  They are unusable there whatever comes
    // after them, so CON.cfg is refused along with CON.
    static const QStringList devices = {
        QStringLiteral("CON"),  QStringLiteral("PRN"),  QStringLiteral("AUX"),
        QStringLiteral("NUL"),  QStringLiteral("COM1"), QStringLiteral("COM2"),
        QStringLiteral("COM3"), QStringLiteral("COM4"), QStringLiteral("COM5"),
        QStringLiteral("COM6"), QStringLiteral("COM7"), QStringLiteral("COM8"),
        QStringLiteral("COM9"), QStringLiteral("LPT1"), QStringLiteral("LPT2"),
        QStringLiteral("LPT3"), QStringLiteral("LPT4"), QStringLiteral("LPT5"),
        QStringLiteral("LPT6"), QStringLiteral("LPT7"), QStringLiteral("LPT8"),
        QStringLiteral("LPT9")};
    const QString stem = clean.section(QLatin1Char('.'), 0, 0).toUpper();
    if (devices.contains(stem))
        return QStringLiteral("%1 is a name Windows keeps for a device, so no file may use it.")
            .arg(clean);

    // Files vclock keeps for itself.  Letting a clock take one of these names
    // would put its settings where the program looks for something else.
    static const QStringList reserved = {QStringLiteral("default"), QStringLiteral("vclock"),
                                         QStringLiteral("vclocks"), QStringLiteral("clocks")};
    if (reserved.contains(clean, Qt::CaseInsensitive))
        return QStringLiteral("%1 is a name vclock keeps for itself, so a clock cannot take it.")
            .arg(clean);

    // 255 bytes is the limit on every filesystem worth worrying about, and it
    // is bytes rather than characters, so an accented name runs out sooner.
    if (clockFileName(clean).toUtf8().size() > 255)
        return QStringLiteral("That name is too long to be a file name.");

    // Compared without case: two clocks a case apart would be two files on
    // Linux but one on Windows and macOS, and the same config directory has to
    // mean the same thing on all of them.
    for (const ClockEntry &entry : registry.clocks) {
        if (entry.file == exceptFile)
            continue;
        if (entry.name.compare(clean, Qt::CaseInsensitive) == 0)
            return QStringLiteral("There is already a clock called %1.").arg(entry.name);
    }

    // Something in the config directory under that name that no clock claims --
    // a config left behind by a clock that was deleted, say.  Taking the name
    // would silently adopt its settings.
    const QString file = clockFileName(clean);
    if (file.compare(exceptFile, Qt::CaseInsensitive) != 0
        && QFile::exists(QDir(configDir()).filePath(file))) {
        return QStringLiteral("There is already a file called %1 in the config directory.")
            .arg(file);
    }

    return QString();
}

QString registryPath()
{
    return QDir(configDir()).filePath(QStringLiteral("vclocks.cfg"));
}

// Where the list used to be kept.  "clocks" is a plausible enough thing for
// someone to call a clock of their own that the name was given the same v as
// everything else here; a list written under the old name is moved across
// rather than being read as somebody's clock.
QString legacyRegistryPath()
{
    return QDir(configDir()).filePath(QStringLiteral("clocks.cfg"));
}

Registry loadRegistry()
{
    // Moved, not copied, so the old name is free for a clock to use and there
    // is only ever one list to keep up to date.
    if (!QFile::exists(registryPath()) && QFile::exists(legacyRegistryPath()))
        QFile::rename(legacyRegistryPath(), registryPath());

    QFile file(registryPath());
    if (!file.open(QIODevice::ReadOnly))
        return discoverExisting();
    const QByteArray raw = file.readAll();
    file.close();

    QJsonParseError err{};
    const QJsonDocument doc = QJsonDocument::fromJson(raw, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject())
        return discoverExisting();

    Registry registry;
    QSet<QString> seen;
    const QJsonObject root = doc.object();
    // Read before the list, so that a file whose clocks turn out to be
    // unusable still hands back the setting rather than quietly resetting it.
    const int hoverDelayMs =
        qBound(0, root.value(QLatin1String("hoverDelayMs")).toInt(kHoverDelayDefaultMs),
               kHoverDelayMaxMs);
    const QJsonArray array = root.value(QLatin1String("clocks")).toArray();
    for (const QJsonValue &value : array) {
        const QJsonObject o = value.toObject();
        ClockEntry entry;
        entry.file = o.value(QLatin1String("file")).toString().trimmed();
        if (entry.file.isEmpty() || seen.contains(entry.file))
            continue;
        // A file name is only ever one we wrote, so anything trying to reach
        // outside the config directory is a hand-edited file and is dropped.
        if (entry.file.contains(QLatin1Char('/')) || entry.file.contains(QLatin1Char('\\')))
            continue;
        seen.insert(entry.file);
        entry.name = o.value(QLatin1String("name")).toString();
        // "autostart" is what this flag was called before showing and
        // starting were the same thing; a file written by that release still
        // says what the user wanted.
        entry.show = o.value(QLatin1String("show"))
                         .toBool(o.value(QLatin1String("autostart")).toBool(true));
        registry.clocks.push_back(entry);
    }

    registry.hoverDelayMs = hoverDelayMs;
    if (registry.clocks.isEmpty()) {
        Registry found = discoverExisting();
        found.hoverDelayMs = hoverDelayMs;
        return found;
    }
    // The default config is always listed: it is what a clock started with no
    // --config writes, so leaving it out would hide a clock the user can see.
    if (registry.indexOfFile(QStringLiteral("default.cfg")) < 0)
        registry.clocks.prepend(defaultEntry());
    return registry;
}

void saveRegistry(const Registry &registry)
{
    QDir().mkpath(configDir());

    QJsonArray array;
    for (const ClockEntry &entry : registry.clocks) {
        QJsonObject o;
        o.insert(QLatin1String("file"), entry.file);
        o.insert(QLatin1String("name"), entry.name);
        o.insert(QLatin1String("show"), entry.show);
        array.append(o);
    }
    QJsonObject root;
    root.insert(QLatin1String("clocks"), array);
    root.insert(QLatin1String("hoverDelayMs"), registry.hoverDelayMs);

    QSaveFile file(registryPath());
    if (!file.open(QIODevice::WriteOnly))
        return;
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    file.commit();
}

QString fallbackClockName(const QString &path)
{
    const QString label = configLabel(path);
    return label.isEmpty() ? QStringLiteral("Default") : label;
}
