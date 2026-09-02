// The settings record, its defaults, and reading/writing it as JSON in a
// platform-native config directory.
#pragma once

#include <QPointF>
#include <QString>
#include <optional>

// Faces that live inside the program rather than on disk.  They are stored in
// the config as "builtin:<name>", which no real path can collide with.
inline const QString kBuiltinFacePrefix = QStringLiteral("builtin:");
inline const QString kDefaultFaceLabel = QStringLiteral("built-in");

inline constexpr int kSizeMin = 50;
inline constexpr int kSizeMaxFallback = 500;  // only used if no screen can be queried

// Hand geometry, as fractions of the usable face radius.  Expressing widths
// as fractions too keeps the hands proportional at every clock size.
inline constexpr double kHandSpan = 0.98;  // the second hand reaches 98% of the face
inline constexpr double kHourLen = 0.62, kMinuteLen = 0.86, kSecondLen = 1.00;
inline constexpr double kHourWidth = 0.072, kMinuteWidth = 0.043, kSecondWidth = 0.029;
inline constexpr double kPinRadius = 0.047;

// Hour indices (the tick marks around the dial).  kMarkOuter is the outer edge
// at 100% position; the size slider grows the marks inwards from there.
inline constexpr double kMarkInner = 0.88, kMarkOuter = 0.98, kMarkWidth = 0.011;

// User-adjustable scaling, as whole percentages of the geometry above.
inline constexpr int kHandScaleMin = 25, kHandScaleMax = 200;
inline constexpr int kMarkScaleMin = 0, kMarkScaleMax = 200;  // 0 hides the indices

struct Config
{
    int size = 400;                                        // widget width in px
    int handScale = 100;                                   // percent of the built-in hand geometry
    int markScale = 100;                                   // percent of the built-in hour-index geometry
    int markPosition = 100;                                // percent of the built-in index radius
    int minuteMarkScale = 50;                              // percent of the hour-index size
    bool quarterMarksOnly = false;                         // draw hour indices at 12/3/6/9 only
    bool alwaysOnTop = false;                              // keep the widget above other windows
    QString faceSvg;                                       // path to a user-supplied face ("" = none)
    bool faceDefault = true;                               // ignore faceSvg and use the built-in face
    QString secondColor = QStringLiteral("#8b0000");       // dark red
    QString hourColor = QStringLiteral("#363d45");         // charcoal
    QString minuteColor = QStringLiteral("#000000");
    bool minuteSameAsHour = false;
    QString faceColor = QStringLiteral("#ffffff");
    bool faceTransparent = false;
    QString wireColor = QStringLiteral("#000000");
    QString hourMarkColor = QStringLiteral("#000000");
    QString minuteMarkColor = QStringLiteral("#000000");

    std::optional<QPointF> center;  // fractions of the canvas; unset = auto
    std::optional<int> x;
    std::optional<int> y;

    // The face file to load, or an empty string for the embedded default.
    QString facePath() const { return faceDefault ? QString() : faceSvg; }

    // Where the hands pivot, as fractions of the canvas (auto = its centre).
    QPointF centerFraction() const { return center.value_or(QPointF(0.5, 0.5)); }

    QString minuteHandColor() const { return minuteSameAsHour ? hourColor : minuteColor; }
};

QString configDir();
QString configPath();

Config loadConfig();
void saveConfig(const Config &cfg);

// Copy everything a preset may change (appearance only: not the window's
// physical size, stacking or placement, which stay the user's).
void copyPresetKeys(const Config &from, Config &to);

// Copy everything "Reset defaults" restores (all but the on-screen position).
void copyResetKeys(const Config &from, Config &to);

int clampPercent(double value, int low, int high, int fallback);
std::optional<QPointF> sanitizeCenter(const std::optional<QPointF> &value);
