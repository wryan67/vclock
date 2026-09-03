// The list of clocks the user has made: which config file each one keeps its
// settings in, what to call it, and whether it is on screen.
//
// Note the file is vclocks.cfg.  It cannot be vclock.cfg: that name already
// belongs to the config an older release wrote, which loadConfig() still reads
// and migrates, so a registry stored there would be mistaken for a clock's
// settings on a machine that has yet to write default.cfg.  It is not plain
// clocks.cfg either, which was its name in an earlier build -- that is a name
// someone might reasonably give a clock of their own.  A list found under the
// old name is renamed on the first read.
#pragma once

#include <QString>
#include <QVector>

struct ClockEntry
{
    // The config file this clock reads and writes.  A bare name lives in the
    // config directory.  This is the clock's identity and never changes once
    // it has been handed out, so renaming is only ever a relabelling.
    QString file;
    // What to call it in the menus and the manage dialog.  May be anything,
    // including a name another clock already uses.
    QString name;
    // Whether the clock is on screen.  It is written out as it changes, so
    // stopping and starting vclock brings back exactly the set that was
    // showing -- there is no separate "open this at startup" flag, because
    // being on screen when you last stopped is the same statement.
    bool show = true;

    // The file's absolute path.
    QString path() const;
};

class Registry
{
public:
    QVector<ClockEntry> clocks;

    int indexOfFile(const QString &file) const;
    int indexOfPath(const QString &path) const;
    const ClockEntry *findPath(const QString &path) const;

    // A file name no clock is using yet, derived from a display name so the
    // config directory stays readable.
    QString uniqueFileFor(const QString &name) const;
};

QString registryPath();

// Reads the registry, creating one from whatever configs are already in the
// config directory the first time it is asked for.  Never returns empty: a
// machine with no configs at all still gets the default clock.
Registry loadRegistry();

void saveRegistry(const Registry &registry);

// The label to show for a clock whose registry entry is missing -- one named
// with --config, say.  Falls back to the config's base name.
QString fallbackClockName(const QString &path);
