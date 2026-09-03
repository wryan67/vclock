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

// Turn a display name into something that reads well as a file name, without
// ever letting it climb out of the config directory.
QString slugify(const QString &name)
{
    QString slug;
    slug.reserve(name.size());
    for (const QChar c : name) {
        if (c.isLetterOrNumber())
            slug += c.toLower();
        else if ((c == QLatin1Char('-') || c == QLatin1Char('_') || c.isSpace())
                 && !slug.endsWith(QLatin1Char('-')))
            slug += QLatin1Char('-');
    }
    while (slug.endsWith(QLatin1Char('-')))
        slug.chop(1);
    // A name of nothing but punctuation, or one that would collide with the
    // files vclock keeps for itself.
    if (slug.isEmpty() || slug == QLatin1String("default") || slug == QLatin1String("vclocks")
        || slug == QLatin1String("vclock"))
        slug = QStringLiteral("clock");
    return slug;
}

ClockEntry defaultEntry()
{
    ClockEntry entry;
    entry.file = QStringLiteral("default.cfg");
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
        if (file == QLatin1String("default.cfg") || file == QLatin1String("vclocks.cfg")
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

QString Registry::uniqueFileFor(const QString &name) const
{
    const QString slug = slugify(name);
    const QDir dir(configDir());
    for (int n = 0;; ++n) {
        const QString candidate =
            n == 0 ? slug + QStringLiteral(".cfg")
                   : slug + QStringLiteral("-") + QString::number(n + 1) + QStringLiteral(".cfg");
        // Free only if no clock claims it and nothing is on disk under it, so
        // a new clock never inherits a config left behind by a deleted one.
        if (indexOfFile(candidate) < 0 && !QFile::exists(dir.filePath(candidate)))
            return candidate;
    }
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
    const QJsonArray array = doc.object().value(QLatin1String("clocks")).toArray();
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

    if (registry.clocks.isEmpty())
        return discoverExisting();
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
