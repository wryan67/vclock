#include "config.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QSaveFile>
#include <QStandardPaths>
#include <QtGlobal>

#include <cmath>

namespace {

const Config kDefaults;

// Old key names, kept readable for configs written by earlier versions.
struct Rename
{
    const char *oldKey;
    const char *newKey;
};
const Rename kRenamed[] = {
    {"ship_color", "face_color"},
    {"ship_transparent", "face_transparent"},
};

bool readBool(const QJsonObject &o, const char *key, bool fallback)
{
    const QJsonValue v = o.value(QLatin1String(key));
    if (v.isBool())
        return v.toBool();
    if (v.isDouble())
        return v.toDouble() != 0.0;
    return fallback;
}

QString readString(const QJsonObject &o, const char *key, const QString &fallback)
{
    const QJsonValue v = o.value(QLatin1String(key));
    return v.isString() ? v.toString() : fallback;
}

// Read a percentage from the config, tolerating junk written by hand.
int readPercent(const QJsonObject &o, const char *key, int low, int high, int fallback)
{
    const QJsonValue v = o.value(QLatin1String(key));
    if (v.isDouble())
        return clampPercent(v.toDouble(), low, high, fallback);
    if (v.isString()) {
        bool ok = false;
        const double d = v.toString().toDouble(&ok);
        if (ok)
            return clampPercent(d, low, high, fallback);
    }
    return fallback;
}

}  // namespace

int clampPercent(double value, int low, int high, int fallback)
{
    if (!std::isfinite(value))
        return fallback;
    const int rounded = static_cast<int>(std::lround(value));
    return qBound(low, rounded, high);
}

// Accept only a pair of fractions inside the canvas; else fall back to auto.
std::optional<QPointF> sanitizeCenter(const std::optional<QPointF> &value)
{
    if (!value.has_value())
        return std::nullopt;
    const double fx = value->x();
    const double fy = value->y();
    if (!std::isfinite(fx) || !std::isfinite(fy))
        return std::nullopt;
    if (fx < 0.0 || fx > 1.0 || fy < 0.0 || fy > 1.0)
        return std::nullopt;
    return value;
}

// Platform-native directory for the settings file.
QString configDir()
{
    QString base;
#if defined(Q_OS_WIN)
    base = qEnvironmentVariable("APPDATA");
    if (base.isEmpty())
        base = QDir::homePath();
#elif defined(Q_OS_MACOS)
    base = QDir::homePath() + QStringLiteral("/Library/Application Support");
#else
    base = qEnvironmentVariable("XDG_CONFIG_HOME");
    if (base.isEmpty())
        base = QDir::homePath() + QStringLiteral("/.config");
#endif
    return QDir(base).filePath(QStringLiteral("vclock"));
}

QString configPath()
{
    return QDir(configDir()).filePath(QStringLiteral("vclock.cfg"));
}

namespace {

// Settings written by earlier releases, read once when the current path has no
// file yet so an upgrade (or a move to a platform-native path) keeps its config.
QStringList legacyConfigPaths()
{
    const QString home = QDir::homePath();
    return {home + QStringLiteral("/.config/vclock/vclock.cfg"),
            home + QStringLiteral("/.config/fclock/fclock.cfg")};
}

}  // namespace

Config loadConfig()
{
    Config cfg;

    // Settings written under an older name or path are still honoured; the next
    // save writes them to the current location.
    QString path = configPath();
    if (!QFile::exists(path)) {
        for (const QString &legacy : legacyConfigPaths()) {
            if (QFile::exists(legacy)) {
                path = legacy;
                break;
            }
        }
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return cfg;
    const QByteArray raw = file.readAll();
    file.close();

    QJsonParseError err{};
    const QJsonDocument doc = QJsonDocument::fromJson(raw, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject())
        return cfg;
    QJsonObject o = doc.object();

    for (const Rename &r : kRenamed) {
        const QLatin1String oldKey(r.oldKey);
        const QLatin1String newKey(r.newKey);
        if (o.contains(oldKey) && !o.contains(newKey)) {
            o.insert(newKey, o.value(oldKey));
            o.remove(oldKey);
        }
    }
    // The marks used to be painted in the wire colour.  A config from before
    // they had their own colours keeps its old look.
    if (o.contains(QLatin1String("wire_color"))) {
        for (const char *key : {"hour_mark_color", "minute_mark_color"}) {
            if (!o.contains(QLatin1String(key)))
                o.insert(QLatin1String(key), o.value(QLatin1String("wire_color")));
        }
    }

    const QJsonValue sizeValue = o.value(QLatin1String("size"));
    if (sizeValue.isDouble())
        cfg.size = static_cast<int>(sizeValue.toDouble());
    else if (sizeValue.isString()) {
        bool ok = false;
        const int parsed = sizeValue.toString().toInt(&ok);
        if (ok)
            cfg.size = parsed;
    }
    cfg.size = qMax(kSizeMin, cfg.size);  // upper bound depends on the screen

    cfg.handScale = readPercent(o, "hand_scale", kHandScaleMin, kHandScaleMax,
                                kDefaults.handScale);
    cfg.markScale = readPercent(o, "mark_scale", kMarkScaleMin, kMarkScaleMax,
                                kDefaults.markScale);
    cfg.markPosition = readPercent(o, "mark_position", kMarkScaleMin, kMarkScaleMax,
                                   kDefaults.markPosition);
    cfg.minuteMarkScale = readPercent(o, "minute_mark_scale", kMarkScaleMin,
                                      kMarkScaleMax, kDefaults.minuteMarkScale);

    cfg.quarterMarksOnly = readBool(o, "quarter_marks_only", kDefaults.quarterMarksOnly);
    cfg.alwaysOnTop = readBool(o, "always_on_top", kDefaults.alwaysOnTop);
    cfg.faceDefault = readBool(o, "face_default", kDefaults.faceDefault);
    cfg.minuteSameAsHour = readBool(o, "minute_same_as_hour", kDefaults.minuteSameAsHour);
    cfg.faceTransparent = readBool(o, "face_transparent", kDefaults.faceTransparent);

    cfg.faceSvg = readString(o, "face_svg", kDefaults.faceSvg);
    cfg.secondColor = readString(o, "second_color", kDefaults.secondColor);
    cfg.hourColor = readString(o, "hour_color", kDefaults.hourColor);
    cfg.minuteColor = readString(o, "minute_color", kDefaults.minuteColor);
    cfg.faceColor = readString(o, "face_color", kDefaults.faceColor);
    cfg.wireColor = readString(o, "wire_color", kDefaults.wireColor);
    cfg.hourMarkColor = readString(o, "hour_mark_color", kDefaults.hourMarkColor);
    cfg.minuteMarkColor = readString(o, "minute_mark_color", kDefaults.minuteMarkColor);

    const QJsonValue centerValue = o.value(QLatin1String("center"));
    if (centerValue.isArray()) {
        const QJsonArray arr = centerValue.toArray();
        if (arr.size() >= 2 && arr.at(0).isDouble() && arr.at(1).isDouble())
            cfg.center = sanitizeCenter(QPointF(arr.at(0).toDouble(), arr.at(1).toDouble()));
    }

    if (o.value(QLatin1String("x")).isDouble())
        cfg.x = static_cast<int>(o.value(QLatin1String("x")).toDouble());
    if (o.value(QLatin1String("y")).isDouble())
        cfg.y = static_cast<int>(o.value(QLatin1String("y")).toDouble());

    if (cfg.faceSvg.isEmpty())
        cfg.faceDefault = true;
    return cfg;
}

void saveConfig(const Config &cfg)
{
    QJsonObject o;
    o.insert(QStringLiteral("size"), cfg.size);
    o.insert(QStringLiteral("hand_scale"), cfg.handScale);
    o.insert(QStringLiteral("mark_scale"), cfg.markScale);
    o.insert(QStringLiteral("mark_position"), cfg.markPosition);
    o.insert(QStringLiteral("minute_mark_scale"), cfg.minuteMarkScale);
    o.insert(QStringLiteral("quarter_marks_only"), cfg.quarterMarksOnly);
    o.insert(QStringLiteral("always_on_top"), cfg.alwaysOnTop);
    o.insert(QStringLiteral("face_svg"), cfg.faceSvg);
    o.insert(QStringLiteral("face_default"), cfg.faceDefault);
    o.insert(QStringLiteral("second_color"), cfg.secondColor);
    o.insert(QStringLiteral("hour_color"), cfg.hourColor);
    o.insert(QStringLiteral("minute_color"), cfg.minuteColor);
    o.insert(QStringLiteral("minute_same_as_hour"), cfg.minuteSameAsHour);
    o.insert(QStringLiteral("face_color"), cfg.faceColor);
    o.insert(QStringLiteral("face_transparent"), cfg.faceTransparent);
    o.insert(QStringLiteral("wire_color"), cfg.wireColor);
    o.insert(QStringLiteral("hour_mark_color"), cfg.hourMarkColor);
    o.insert(QStringLiteral("minute_mark_color"), cfg.minuteMarkColor);

    if (cfg.center.has_value()) {
        QJsonArray arr;
        arr.append(cfg.center->x());
        arr.append(cfg.center->y());
        o.insert(QStringLiteral("center"), arr);
    } else {
        o.insert(QStringLiteral("center"), QJsonValue::Null);
    }
    o.insert(QStringLiteral("x"), cfg.x.has_value() ? QJsonValue(*cfg.x) : QJsonValue::Null);
    o.insert(QStringLiteral("y"), cfg.y.has_value() ? QJsonValue(*cfg.y) : QJsonValue::Null);

    const QString dir = configDir();
    if (!QDir().mkpath(dir)) {
        qWarning("WARNING: could not create %s", qPrintable(dir));
        return;
    }
    // QSaveFile writes to a temporary and renames it into place, which is the
    // same atomic swap the Python version did by hand.
    QSaveFile out(configPath());
    if (!out.open(QIODevice::WriteOnly)
        || out.write(QJsonDocument(o).toJson(QJsonDocument::Indented)) < 0
        || !out.commit()) {
        qWarning("WARNING: could not save %s", qPrintable(configPath()));
    }
}

void copyPresetKeys(const Config &from, Config &to)
{
    to.handScale = from.handScale;
    to.markScale = from.markScale;
    to.markPosition = from.markPosition;
    to.minuteMarkScale = from.minuteMarkScale;
    to.quarterMarksOnly = from.quarterMarksOnly;
    to.faceSvg = from.faceSvg;
    to.faceDefault = from.faceDefault;
    to.secondColor = from.secondColor;
    to.hourColor = from.hourColor;
    to.minuteColor = from.minuteColor;
    to.minuteSameAsHour = from.minuteSameAsHour;
    to.faceColor = from.faceColor;
    to.faceTransparent = from.faceTransparent;
    to.wireColor = from.wireColor;
    to.hourMarkColor = from.hourMarkColor;
    to.minuteMarkColor = from.minuteMarkColor;
}

void copyResetKeys(const Config &from, Config &to)
{
    copyPresetKeys(from, to);
    to.size = from.size;
    to.alwaysOnTop = from.alwaysOnTop;
    to.center = from.center;
}
