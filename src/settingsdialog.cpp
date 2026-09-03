#include "settingsdialog.h"

#include "clockwindow.h"
#include "colorbutton.h"
#include "face.h"
#include "render.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontMetrics>
#include <QGridLayout>
#include <QGroupBox>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QScrollBar>
#include <QSlider>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace {

// Where the file chooser starts looking for user-supplied faces.
QString faceDir()
{
    return QDir::homePath() + QStringLiteral("/Downloads");
}

QString hexOf(const QColor &color)
{
    return color.name(QColor::HexRgb);
}

QLabel *makeReadout(int digits)
{
    auto *label = new QLabel;
    label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    const int width = label->fontMetrics().horizontalAdvance(QString(digits, QLatin1Char('0')));
    label->setMinimumWidth(width);
    return label;
}

}  // namespace

SettingsDialog::SettingsDialog(ClockWindow *clock)
    : QDialog(clock, Qt::Dialog)
    , m_clock(clock)
{
    // Not modal, so the clock stays interactive and the dialog can be moved
    // around freely while previewing changes.
    setWindowTitle(QStringLiteral("Clock Settings"));
    setModal(false);
    setWindowFlag(Qt::WindowStaysOnTopHint, false);

    const Config &cfg = m_clock->cfg();
    m_minuteOwn = cfg.minuteColor;
    m_faceOwn = cfg.faceColor;

    auto *outer = new QVBoxLayout(this);

    // Every control lives on this widget, which the scroll area below pans over
    // when the dialog is taller or wider than the screen it opens on.
    auto *content = new QWidget;
    auto *grid = new QGridLayout(content);
    grid->setHorizontalSpacing(10);
    grid->setVerticalSpacing(8);
    grid->setContentsMargins(12, 12, 12, 12);

    auto *scroll = new QScrollArea(this);
    scroll->setWidget(content);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    // Scroll bars appear only when something is actually out of view. A
    // permanently visible bar would eat width on every setup that fits and
    // suggest there is more below when there is not.
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    // The viewport would otherwise paint in the base (text-entry) colour rather
    // than the dialog's own background.
    scroll->viewport()->setAutoFillBackground(false);
    content->setAutoFillBackground(false);
    outer->addWidget(scroll, 1);

    int row = 0;

    // ------------------------------------------------------------- presets
    addLabel(grid, QStringLiteral("Presets"), row);
    auto *presetBox = new QHBoxLayout;
    presetBox->setSpacing(6);
    const qreal dpr = devicePixelRatioF();
    for (const Preset &preset : presets()) {
        auto *button = new QToolButton(this);
        button->setIcon(QIcon(presetThumbnail(preset.values, kPresetThumb, dpr)));
        button->setIconSize(QSize(kPresetThumb, kPresetThumb));
        button->setAutoRaise(false);
        button->setToolTip(preset.name + QStringLiteral(" \u2014 ") + preset.tip);
        connect(button, &QToolButton::clicked, this,
                [this, &preset] { onPresetClicked(preset); });
        presetBox->addWidget(button);
    }
    presetBox->addStretch(1);
    grid->addLayout(presetBox, row, 1, 1, 3);
    ++row;

    // --------------------------------------------------------- face chooser
    addLabel(grid, QStringLiteral("Clock face svg"), row);
    auto *faceRow = new QHBoxLayout;
    faceRow->setSpacing(6);
    m_faceEdit = new QLineEdit(this);
    m_faceEdit->setReadOnly(true);
    m_faceEdit->setPlaceholderText(QStringLiteral("(none)"));
    if (!cfg.faceSvg.isEmpty() && QFileInfo::exists(cfg.faceSvg)) {
        m_chosenFile = cfg.faceSvg;
        m_faceEdit->setText(cfg.faceSvg);
        m_faceEdit->setToolTip(cfg.faceSvg);
    }
    m_browse = new QPushButton(QStringLiteral("Browse\u2026"), this);
    connect(m_browse, &QPushButton::clicked, this, &SettingsDialog::onBrowse);
    faceRow->addWidget(m_faceEdit, 1);
    faceRow->addWidget(m_browse, 0);
    grid->addLayout(faceRow, row, 1, 1, 3);
    ++row;

    // -------------------------------------------------------------- sliders

    const int maxSize = m_clock->maxSize();
    m_size = addSlider(grid, row++, QStringLiteral("Clock size (px)"), cfg.size, kSizeMin,
                       maxSize, false);
    m_handScale = addSlider(grid, row++, QStringLiteral("Hand size (%)"), cfg.handScale,
                            kHandScaleMin, kHandScaleMax, true);
    m_markScale = addSlider(grid, row++, QStringLiteral("Hour mark size (%)"), cfg.markScale,
                            kMarkScaleMin, kMarkScaleMax, true);
    m_markPosition = addSlider(grid, row++, QStringLiteral("Hour mark position (%)"),
                               cfg.markPosition, kMarkScaleMin, kMarkScaleMax, true);
    m_minuteMarkScale = addSlider(grid, row++, QStringLiteral("Minute mark size (%)"),
                                  cfg.minuteMarkScale, kMarkScaleMin, kMarkScaleMax, true);
    m_minuteMarkScale->setToolTip(
        QStringLiteral("Percentage of the hour mark size. 0 hides the minute marks."));

    // The three independent options share one row, spread across the dialog:
    // stacked they read as a list of three unrelated things taking three rows
    // to say it.
    m_quarterMarks = new QCheckBox(QStringLiteral("quarter marks only"), this);
    m_quarterMarks->setChecked(cfg.quarterMarksOnly);
    m_quarterMarks->setToolTip(QStringLiteral(
        "Draw full indices at 12, 3, 6 and 9 only; the other hours drop to the minute track."));

    m_smoothSweep = new QCheckBox(QStringLiteral("Smooth sweep hands"), this);
    m_smoothSweep->setChecked(cfg.smoothSweep);
    m_smoothSweep->setToolTip(QStringLiteral(
        "Sweep the hands at 60 fps, each at the exact angle for the current millisecond, "
        "instead of stepping them once a second. Costs noticeably more CPU on a large clock."));

    m_reverseTime = new QCheckBox(QStringLiteral("Reverse time"), this);
    m_reverseTime->setChecked(cfg.reverseTime);
    m_reverseTime->setToolTip(QStringLiteral(
        "Run the hands anticlockwise. The clock still keeps the correct time, but each "
        "hand is mirrored about the 12, so you read it in a mirror."));

    auto *optionRow = new QHBoxLayout;
    optionRow->setContentsMargins(0, 0, 0, 0);
    optionRow->addWidget(m_quarterMarks);
    optionRow->addStretch(1);
    optionRow->addWidget(m_smoothSweep);
    optionRow->addStretch(1);
    optionRow->addWidget(m_reverseTime);
    grid->addLayout(optionRow, row, 1, 1, 3);
    ++row;

    // --------------------------------------------------------------- colours
    // Three groups, because these are three separate jobs: what the hands look
    // like, what the dial's markings look like, and what the artwork behind
    // them looks like.  Ungrouped it was one undifferentiated run of swatches.
    auto *handsBox = new QGroupBox(QStringLiteral("Hands"), this);
    auto *handsGrid = new QGridLayout(handsBox);
    handsGrid->setHorizontalSpacing(10);
    int hrow = 0;

    addLabel(handsGrid, QStringLiteral("Second"), hrow);
    m_second = new ColorButton(QColor(cfg.secondColor), false,
                               QStringLiteral("Second hand color"), this);
    handsGrid->addWidget(m_second, hrow, 1, Qt::AlignLeft);
    ++hrow;

    addLabel(handsGrid, QStringLiteral("Hour"), hrow);
    m_hour = new ColorButton(QColor(cfg.hourColor), false, QStringLiteral("Hour hand color"),
                             this);
    handsGrid->addWidget(m_hour, hrow, 1, Qt::AlignLeft);
    ++hrow;

    addLabel(handsGrid, QStringLiteral("Minute"), hrow);
    m_minute = new ColorButton(QColor(cfg.minuteColor), false,
                               QStringLiteral("Minute hand color"), this);
    m_minuteSame = new QCheckBox(QStringLiteral("same as hour"), this);
    m_minuteSame->setChecked(cfg.minuteSameAsHour);
    handsGrid->addLayout(withOption(m_minute, m_minuteSame), hrow, 1);
    ++hrow;

    // The pivot is a property of the hands, so it belongs with them.
    addLabel(handsGrid, QStringLiteral("Center"), hrow);
    m_pick = new QPushButton(QStringLiteral("Pick on clock\u2026"), this);
    m_pick->setToolTip(QStringLiteral(
        "Drag on the clock face, or use the arrow keys to move the pivot a pixel "
        "at a time (hold Shift for 10). Enter accepts, Esc cancels."));
    connect(m_pick, &QPushButton::clicked, this, [this] { m_clock->startPicking(); });
    m_centerAuto = new QCheckBox(QStringLiteral("auto"), this);
    m_centerAuto->setToolTip(QStringLiteral("Pivot on the centre of the canvas."));
    m_centerAuto->setChecked(!m_clock->cfg().center.has_value());
    connect(m_centerAuto, &QCheckBox::toggled, this, [this](bool on) {
        if (on) {
            m_clock->stopPicking();
            m_clock->setCenter(std::nullopt, true);
        }
        refreshCenter();
    });
    handsGrid->addLayout(withOption(m_pick, m_centerAuto), hrow, 1);
    ++hrow;

    m_centerLabel = new QLabel(this);
    m_centerLabel->setTextFormat(Qt::RichText);
    handsGrid->addWidget(m_centerLabel, hrow, 0, 1, 2);
    ++hrow;

    auto *marksBox = new QGroupBox(QStringLiteral("Marks"), this);
    auto *marksGrid = new QGridLayout(marksBox);
    marksGrid->setHorizontalSpacing(10);
    int mrow = 0;

    addLabel(marksGrid, QStringLiteral("Hour"), mrow);
    m_hourMark = new ColorButton(QColor(cfg.hourMarkColor), false,
                                 QStringLiteral("Hour mark color"), this);
    marksGrid->addWidget(m_hourMark, mrow, 1, Qt::AlignLeft);
    ++mrow;

    addLabel(marksGrid, QStringLiteral("Minute"), mrow);
    m_minuteMark = new ColorButton(QColor(cfg.minuteMarkColor), false,
                                   QStringLiteral("Minute mark color"), this);
    marksGrid->addWidget(m_minuteMark, mrow, 1, Qt::AlignLeft);
    ++mrow;
    marksGrid->setRowStretch(mrow, 1);

    auto *faceBox = new QGroupBox(QStringLiteral("Face"), this);
    auto *faceGrid = new QGridLayout(faceBox);
    faceGrid->setHorizontalSpacing(10);
    int frow = 0;

    // The mode governs the two swatches under it, so it sits with them.
    addLabel(faceGrid, QStringLiteral("Coloring"), frow);
    m_colorMode = new QComboBox(this);
    m_colorMode->addItem(QStringLiteral("Recolor"), true);
    m_colorMode->addItem(QStringLiteral("Original"), false);
    m_colorMode->setCurrentIndex(cfg.faceRecolor ? 0 : 1);
    m_colorMode->setToolTip(QStringLiteral(
        "Recolor maps the artwork's shading onto the face and wire colors below, so "
        "the drawing comes out in your colors with its shading intact.\n\n"
        "Original draws the file exactly as it was authored, which suits a picture "
        "that already has colors of its own. The face and wire colors then do not "
        "apply; the hands and marks still do."));
    faceGrid->addWidget(m_colorMode, frow, 1, Qt::AlignLeft);
    ++frow;

    addLabel(faceGrid, QStringLiteral("Face color"), frow);
    // useAlpha lets the swatch show the checkerboard when transparent.
    m_face = new ColorButton(QColor(cfg.faceColor), true, QStringLiteral("Face color"), this);
    m_faceTransparent = new QCheckBox(QStringLiteral("transparent"), this);
    m_faceTransparent->setChecked(cfg.faceTransparent);
    faceGrid->addLayout(withOption(m_face, m_faceTransparent), frow, 1);
    ++frow;

    addLabel(faceGrid, QStringLiteral("Wire color"), frow);
    m_wire = new ColorButton(QColor(cfg.wireColor), false, QStringLiteral("Wire color"), this);
    faceGrid->addWidget(m_wire, frow, 1, Qt::AlignLeft);
    ++frow;
    faceGrid->setRowStretch(frow, 1);

    // Hands on the left, and the two shorter groups stacked beside it, so
    // neither column ends in a long stretch of nothing.
    auto *groupRow = new QHBoxLayout;
    groupRow->setSpacing(10);
    auto *rightColumn = new QVBoxLayout;
    rightColumn->setSpacing(10);
    rightColumn->addWidget(marksBox);
    rightColumn->addWidget(faceBox);
    groupRow->addWidget(handsBox, 1);
    groupRow->addLayout(rightColumn, 1);
    grid->addLayout(groupRow, row, 0, 1, 4);
    ++row;

    // --------------------------------------------------------------- buttons
    auto *buttons = new QDialogButtonBox(this);
    QPushButton *save = buttons->addButton(QStringLiteral("Save"),
                                           QDialogButtonBox::AcceptRole);
    buttons->addButton(QStringLiteral("Cancel"), QDialogButtonBox::RejectRole);
    save->setDefault(true);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    outer->addWidget(buttons);

    // ------------------------------------------------------------- wiring up
    for (QSlider *slider : {m_size, m_handScale, m_markScale, m_markPosition,
                            m_minuteMarkScale}) {
        connect(slider, &QSlider::valueChanged, this, [this] { onChanged(); });
    }
    for (ColorButton *button : {m_second, m_hour, m_minute, m_face, m_wire, m_hourMark,
                                m_minuteMark}) {
        connect(button, &ColorButton::colorSet, this, [this, button] { onChanged(button); });
    }
    for (QCheckBox *box : {m_minuteSame, m_faceTransparent, m_quarterMarks,
                           m_smoothSweep, m_reverseTime}) {
        connect(box, &QCheckBox::toggled, this, [this] { onChanged(); });
    }
    connect(m_colorMode, &QComboBox::currentIndexChanged, this, [this] { onChanged(); });

    syncSwatches();
    refreshCenter();
    m_live = true;

    resizeToFit(content, scroll, buttons);
}

// Open at the dialog's natural size where the screen allows it, and no larger
// than the screen where it does not -- past that point the scroll area takes
// over. This has to be done by hand because QScrollArea::sizeHint() clamps
// itself to a couple of dozen text lines, so simply calling adjustSize() would
// open the dialog already scrolled no matter how much room there is.
void SettingsDialog::resizeToFit(QWidget *content, QScrollArea *scroll,
                                 QWidget *buttons)
{
    content->ensurePolished();
    if (QLayout *layout = content->layout())
        layout->activate();

    const QMargins margins = layout()->contentsMargins();
    const QSize natural = content->sizeHint();
    QSize want(natural.width() + margins.left() + margins.right(),
               natural.height() + buttons->sizeHint().height()
                   + layout()->spacing() + margins.top() + margins.bottom());

    const QScreen *screen = QGuiApplication::screenAt(m_clock->frameGeometry().center());
    if (!screen)
        screen = QGuiApplication::primaryScreen();
    if (screen) {
        // Leave room for the title bar and any panels the working area does not
        // already account for, so the dialog stays fully reachable.
        const QSize room = screen->availableGeometry().size() - QSize(0, 64);
        if (want.height() > room.height()) {
            want.setHeight(std::max(room.height(), scroll->minimumSizeHint().height()));
            // Losing height to a vertical bar can push the content wider than
            // the viewport, which would otherwise bring on a horizontal bar too.
            want.setWidth(want.width() + scroll->verticalScrollBar()->sizeHint().width());
        }
        want = want.boundedTo(room.expandedTo(QSize(0, 0)));
    }

    resize(want);
}

// A control with the checkbox that qualifies it sitting right beside it, rather
// than adrift in a far column where it reads as belonging to nothing.
QHBoxLayout *SettingsDialog::withOption(QWidget *control, QWidget *box)
{
    auto *layout = new QHBoxLayout;
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);
    layout->addWidget(control, 0);
    layout->addWidget(box, 0);
    layout->addStretch(1);
    return layout;
}

QLabel *SettingsDialog::addLabel(QGridLayout *grid, const QString &text, int row, int col)
{
    auto *label = new QLabel(text, this);
    label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    grid->addWidget(label, row, col);
    return label;
}

// A slider laid out like the Python version: value readout on the left, and
// the range spelled out underneath so the available span is obvious.
QSlider *SettingsDialog::addSlider(QGridLayout *grid, int row, const QString &caption,
                                   int value, int low, int high, bool markHundred)
{
    addLabel(grid, caption, row);

    auto *slider = new QSlider(Qt::Horizontal, this);
    slider->setRange(low, high);
    slider->setValue(qBound(low, value, high));
    slider->setMinimumWidth(240);
    slider->setTickPosition(QSlider::TicksBelow);
    slider->setTickInterval(qMax(1, (high - low) / 10));
    slider->setPageStep(qMax(1, (high - low) / 10));

    const int digits = QString::number(high).size();
    QLabel *readout = makeReadout(digits);
    readout->setText(QString::number(slider->value()));
    connect(slider, &QSlider::valueChanged, readout,
            [readout](int v) { readout->setText(QString::number(v)); });

    auto *marks = new QHBoxLayout;
    marks->setContentsMargins(0, 0, 0, 0);
    const auto smallLabel = [this](const QString &text) {
        auto *label = new QLabel(QStringLiteral("<small>%1</small>").arg(text), this);
        label->setTextFormat(Qt::RichText);
        return label;
    };
    marks->addWidget(smallLabel(QString::number(low)));
    marks->addStretch(1);
    if (markHundred && low < 100 && high > 100) {
        marks->addWidget(smallLabel(QStringLiteral("100")));
        marks->addStretch(1);
    }
    marks->addWidget(smallLabel(QString::number(high)));

    auto *column = new QVBoxLayout;
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(0);
    column->addWidget(slider);
    column->addLayout(marks);

    auto *box = new QHBoxLayout;
    box->setSpacing(8);
    box->addWidget(readout, 0);
    box->addLayout(column, 1);
    grid->addLayout(box, row, 1, 1, 3);
    return slider;
}

void SettingsDialog::onBrowse()
{
    QString start = m_chosenFile;
    if (start.isEmpty() || !QFileInfo::exists(start))
        start = QFileInfo::exists(faceDir()) ? faceDir() : QDir::homePath();
    const QString chosen = QFileDialog::getOpenFileName(
        this, QStringLiteral("Choose a clock face SVG"), start,
        QStringLiteral("SVG images (*.svg);;All files (*)"));
    if (chosen.isEmpty())
        return;
    m_chosenFile = chosen;
    m_faceEdit->setText(chosen);
    m_faceEdit->setToolTip(chosen);
    m_hasFaceOverride = false;  // an explicit file beats a preset's face
    m_faceOverride.clear();
    onChanged();
}

void SettingsDialog::onPresetClicked(const Preset &preset)
{
    Config values = m_clock->cfg();
    copyPresetKeys(preset.values, values);
    applyPreset(values);
}

// Snap every appearance control to a preset, then preview it once.
void SettingsDialog::applyPreset(const Config &values)
{
    const bool wasLive = m_live;
    m_live = false;  // move the widgets without a preview per widget
    m_hasFaceOverride = true;
    m_faceOverride = values.faceSvg;
    // Presets only ever use the built-in faces, so whatever file the chooser
    // was holding must not win over the preset's own face.
    m_chosenFile.clear();
    m_faceEdit->clear();
    m_faceEdit->setToolTip(QString());

    // A preset is a whole default clock, so it restores the size and the hand
    // pivot too.  The centre lives on the clock rather than in a widget, so it
    // has to be set there; refreshCenter() then re-syncs the "auto" checkbox.
    m_clock->stopPicking();
    m_clock->setCenter(values.center, false);

    m_size->setValue(values.size);
    m_handScale->setValue(values.handScale);
    m_markScale->setValue(values.markScale);
    m_markPosition->setValue(values.markPosition);
    m_minuteMarkScale->setValue(values.minuteMarkScale);
    m_quarterMarks->setChecked(values.quarterMarksOnly);
    m_minuteSame->setChecked(values.minuteSameAsHour);
    m_faceTransparent->setChecked(values.faceTransparent);
    m_colorMode->setCurrentIndex(values.faceRecolor ? 0 : 1);

    m_faceOwn = values.faceColor;
    m_minuteOwn = values.minuteColor;
    m_second->setColor(QColor(values.secondColor));
    m_hour->setColor(QColor(values.hourColor));
    m_wire->setColor(QColor(values.wireColor));
    m_hourMark->setColor(QColor(values.hourMarkColor));
    m_minuteMark->setColor(QColor(values.minuteMarkColor));

    m_live = wasLive;
    onChanged();
}

void SettingsDialog::onChanged(const QObject *sender)
{
    if (sender == m_minute && !m_minuteSame->isChecked())
        m_minuteOwn = hexOf(m_minute->color());
    if (sender == m_face && !m_faceTransparent->isChecked())
        m_faceOwn = hexOf(m_face->color());
    syncSwatches();
    if (m_live) {
        m_clock->applySettings(values());
        refreshCenter();
    }
}

// Mirror the hour colour into the minute swatch and show the alpha
// checkerboard on the face swatch, matching the checkbox states.  Setting a
// colour programmatically does not emit colorSet(), so this cannot recurse.
void SettingsDialog::syncSwatches()
{
    const bool same = m_minuteSame->isChecked();
    m_minute->setEnabled(!same);
    m_minute->setColor(same ? m_hour->color() : QColor(m_minuteOwn));

    // The face and wire colours are the recolour's two ends, so they mean
    // nothing at all when the artwork is drawn as authored.
    const bool recolor = recolorMode();
    const bool clear = m_faceTransparent->isChecked();
    m_face->setEnabled(recolor && !clear);
    m_wire->setEnabled(recolor);
    m_faceTransparent->setEnabled(recolor);
    QColor faceColor(m_faceOwn);
    if (!faceColor.isValid())
        faceColor = QColor(Qt::white);
    faceColor.setAlpha(clear ? 0 : 255);
    m_face->setColor(faceColor);
}

void SettingsDialog::refreshCenter()
{
    const bool autoCenter = !m_clock->cfg().center.has_value();
    if (m_centerAuto->isChecked() != autoCenter) {
        const QSignalBlocker blocker(m_centerAuto);
        m_centerAuto->setChecked(autoCenter);
    }
    m_pick->setEnabled(true);
    const QPointF center = m_clock->centerPixels();
    m_centerLabel->setText(
        QStringLiteral("<small>%1: %2, %3 px&nbsp;&nbsp;(radius %4)</small>")
            .arg(autoCenter ? QStringLiteral("auto") : QStringLiteral("manual"),
                 QString::number(std::lround(center.x())),
                 QString::number(std::lround(center.y())),
                 QString::number(std::lround(m_clock->handRadius()))));

    // With no file chosen the field would otherwise read "(none)", which says
    // nothing about which built-in face a preset just loaded.
    m_faceEdit->setPlaceholderText(m_clock->faceLabel());
}

bool SettingsDialog::recolorMode() const
{
    return m_colorMode->currentData().toBool();
}

QString SettingsDialog::faceSvg() const
{
    if (!m_chosenFile.isEmpty())
        return m_chosenFile;
    if (m_hasFaceOverride)
        return m_faceOverride;
    return m_clock->cfg().faceSvg;
}

// Fall back to the built-in face when no file has been chosen.  Getting back
// here is a matter of clicking a preset, which is what the presets are for.
bool SettingsDialog::faceDefault() const
{
    return faceSvg().isEmpty();
}

Config SettingsDialog::values() const
{
    Config out = m_clock->cfg();  // size/stacking/placement stay the clock's
    out.size = m_size->value();
    out.handScale = m_handScale->value();
    out.markScale = m_markScale->value();
    out.markPosition = m_markPosition->value();
    out.minuteMarkScale = m_minuteMarkScale->value();
    out.quarterMarksOnly = m_quarterMarks->isChecked();
    out.faceSvg = faceSvg();
    out.faceDefault = faceDefault();
    out.secondColor = hexOf(m_second->color());
    out.hourColor = hexOf(m_hour->color());
    out.minuteColor = m_minuteOwn;
    out.minuteSameAsHour = m_minuteSame->isChecked();
    out.smoothSweep = m_smoothSweep->isChecked();
    out.reverseTime = m_reverseTime->isChecked();
    out.faceColor = m_faceOwn;
    out.faceTransparent = m_faceTransparent->isChecked();
    out.faceRecolor = recolorMode();
    out.wireColor = hexOf(m_wire->color());
    out.hourMarkColor = hexOf(m_hourMark->color());
    out.minuteMarkColor = hexOf(m_minuteMark->color());
    return out;
}
