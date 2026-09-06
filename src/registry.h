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
    // config directory.  It is the clock's name with .cfg on the end, and it
    // moves when the clock is renamed, so that what is in the config directory
    // can be read off the list of clocks and back again.
    QString file;
    // What to call it in the menus and the manage dialog.  Because it is also
    // the file name, it has to be a name a file can have, and no two clocks
    // may share one -- see clockNameError().
    QString name;
    // Whether the clock is on screen.  It is written out as it changes, so
    // stopping and starting vclock brings back exactly the set that was
    // showing -- there is no separate "open this at startup" flag, because
    // being on screen when you last stopped is the same statement.
    bool show = true;

    // The file's absolute path.
    QString path() const;
};

// How long the pointer has to rest on a clock before the date bubble appears.
// Long enough that it is a deliberate act rather than something that happens
// on the way past.
constexpr int kHoverDelayDefaultMs = 3000;
constexpr int kHoverDelayMaxMs = 10000;

class Registry
{
public:
    QVector<ClockEntry> clocks;

    // Settings that belong to the program rather than to any one clock, and so
    // are kept here beside the list rather than repeated in every config.
    //
    // The wait before a clock shows the date under the pointer.  Zero turns the
    // bubble off: there is no separate switch, because a wait of no time at all
    // is not something anybody would ask for on purpose, which leaves the value
    // free to mean the one other thing you might want.
    int hoverDelayMs = kHoverDelayDefaultMs;

    int indexOfFile(const QString &file) const;
    int indexOfPath(const QString &path) const;
    const ClockEntry *findPath(const QString &path) const;
};

// The config the program falls back to when no clock has been named: the file
// a first run writes, what --clock with no argument means, and the one clock
// that is always in the list.  It is fixed, which is why "default" is not a
// name a clock of the user's own may take.
QString defaultClockFile();
bool isDefaultClockFile(const QString &file);

// The file a clock of this name keeps its settings in.
QString clockFileName(const QString &name);

// A name as typed, made into the name that will be stored: surrounding space
// taken off, and a .cfg the user need not have typed taken off with it, since
// the program puts that on itself and a clock called "kitchen.cfg" would
// otherwise end up in kitchen.cfg.cfg.
QString cleanClockName(const QString &name);

// Why this name cannot be used, in a sentence fit to show the user, or an
// empty string if it can.  `exceptFile` is the clock being renamed, so that
// keeping its own name does not count as a clash with itself.
QString clockNameError(const QString &name, const Registry &registry,
                       const QString &exceptFile = QString());

QString registryPath();

// Reads the registry, creating one from whatever configs are already in the
// config directory the first time it is asked for.  Never returns empty: a
// machine with no configs at all still gets the default clock.
Registry loadRegistry();

void saveRegistry(const Registry &registry);

// The label to show for a clock whose registry entry is missing -- one named
// with --config, say.  Falls back to the config's base name.
QString fallbackClockName(const QString &path);
