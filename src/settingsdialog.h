// The Settings window: appearance presets, the face chooser, the geometry
// sliders and the colour swatches, all previewing live on the clock.
#pragma once

#include "config.h"
#include "render.h"

#include <QDialog>
#include <QString>

class ClockWindow;
class ColorButton;
class QCheckBox;
class QGridLayout;
class QLabel;
class QLineEdit;
class QScrollArea;
class QSlider;

class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(ClockWindow *clock);

    // The settings the controls currently describe, on top of the clock's own
    // window size, stacking and placement.
    Config values() const;

    // Show the pivot in canvas pixels, and keep the "auto" box in sync.
    void refreshCenter();

private:
    QLabel *addLabel(QGridLayout *grid, const QString &text, int row);
    QSlider *addSlider(QGridLayout *grid, int row, const QString &caption, int value, int low,
                       int high, bool markHundred);

    // Size the dialog to its contents, or to the screen when the contents do
    // not fit and the scroll area has to take over.
    void resizeToFit(QWidget *content, QScrollArea *scroll, QWidget *buttons);

    void onChanged(const QObject *sender = nullptr);
    void onBrowse();
    void onPresetClicked(const Preset &preset);
    void applyPreset(const Config &values);
    void syncSwatches();

    QString faceSvg() const;
    bool faceDefault() const;

    ClockWindow *m_clock = nullptr;
    bool m_live = false;

    // An explicit file beats a preset's face; a preset's face beats the config.
    QString m_chosenFile;
    QString m_faceOverride;
    bool m_hasFaceOverride = false;

    QLineEdit *m_faceEdit = nullptr;
    class QPushButton *m_browse = nullptr;
    QCheckBox *m_faceIsDefault = nullptr;

    QSlider *m_size = nullptr;
    QSlider *m_handScale = nullptr;
    QSlider *m_markScale = nullptr;
    QSlider *m_markPosition = nullptr;
    QSlider *m_minuteMarkScale = nullptr;
    QCheckBox *m_quarterMarks = nullptr;
    QCheckBox *m_smoothSweep = nullptr;

    ColorButton *m_second = nullptr;
    ColorButton *m_hour = nullptr;
    ColorButton *m_minute = nullptr;
    ColorButton *m_face = nullptr;
    ColorButton *m_wire = nullptr;
    ColorButton *m_hourMark = nullptr;
    ColorButton *m_minuteMark = nullptr;
    QCheckBox *m_minuteSame = nullptr;
    QCheckBox *m_faceTransparent = nullptr;

    // The user's own colours are remembered while the "same as hour" and
    // "transparent" boxes are ticked, so unticking them brings the colour back.
    QString m_minuteOwn;
    QString m_faceOwn;

    class QPushButton *m_pick = nullptr;
    QCheckBox *m_centerAuto = nullptr;
    QLabel *m_centerLabel = nullptr;
};
