#include "settingsdialog.h"

#include "clockwindow.h"
#include "colorbutton.h"
#include "face.h"
#include "render.h"
#include "icons.h"

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
#include <QSpinBox>
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

// The widest value any of these sliders can show, so that every box in the
// group is the same size however few digits its own range needs.  A slider that
// only ever reaches 200 still gets a box wide enough for the clock size, which
// is what stops the column looking ragged.
constexpr int kReadoutDigits = 4;

// The value beside a slider, which can also be typed into.  Editing it is the
// only way to set an exact number on a slider whose range is wider than the
// pixels it is drawn in -- clock size steps several pixels per pixel of travel.
QSpinBox *makeReadout(int low, int high)
{
    auto *box = new QSpinBox;
    box->setRange(low, high);
    box->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    // Typing is clamped rather than rejected: QSpinBox keeps what is typed
    // inside the range on its own, and correctFromNearestValue tidies a
    // half-finished number when focus leaves rather than reverting it.
    box->setKeyboardTracking(false);
    box->setCorrectionMode(QAbstractSpinBox::CorrectToNearestValue);
    box->setButtonSymbols(QAbstractSpinBox::NoButtons);
    const int width = box->fontMetrics().horizontalAdvance(
                          QString(kReadoutDigits, QLatin1Char('0')))
                      + 14;
    box->setFixedWidth(width);
    return box;
}

}  // namespace

SettingsDialog::SettingsDialog(ClockWindow *clock)
    : QDialog(clock, Qt::Dialog)
    , m_clock(clock)
{
    // Not modal, so the clock stays interactive and the dialog can be moved
    // around freely while previewing changes.
    const QString which = m_clock->configName();
    setWindowTitle(which.isEmpty() ? QStringLiteral("Clock Settings")
                                   : QStringLiteral("Clock Settings \u2014 ") + which);
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

    // --------------------------------------------------- face chooser section
    // Both ways of picking a face -- a ready-made look or a file of your own --
    // belong together, since either one replaces the other.
    auto *chooserBox = new QGroupBox(QStringLiteral("Clock face"), this);
    auto *chooserGrid = new QGridLayout(chooserBox);
    chooserGrid->setHorizontalSpacing(10);
    int crow = 0;

    addLabel(chooserGrid, QStringLiteral("Presets"), crow);
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
    chooserGrid->addLayout(presetBox, crow, 1);
    ++crow;

    addLabel(chooserGrid, QStringLiteral("Image"), crow);
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
    chooserGrid->addLayout(faceRow, crow, 1);
    ++crow;
    chooserGrid->setColumnStretch(1, 1);

    grid->addWidget(chooserBox, row, 0, 1, 4);
    ++row;

    // --------------------------------------------------------- sizes section
    auto *sizesBox = new QGroupBox(QStringLiteral("Sizes"), this);
    auto *sizesGrid = new QGridLayout(sizesBox);
    sizesGrid->setHorizontalSpacing(10);
    sizesGrid->setVerticalSpacing(8);
    int srow = 0;

    const int maxSize = m_clock->maxSize();
    m_size = addSlider(sizesGrid, srow++, QStringLiteral("Clock size (px)"), cfg.size, kSizeMin,
                       maxSize, false);
    m_handScale = addSlider(sizesGrid, srow++, QStringLiteral("Hand size (%)"), cfg.handScale,
                            kHandScaleMin, kHandScaleMax, true);
    m_markScale = addSlider(sizesGrid, srow++, QStringLiteral("Hour mark size (%)"),
                            cfg.markScale, kMarkScaleMin, kMarkScaleMax, true);
    m_markPosition = addSlider(sizesGrid, srow++, QStringLiteral("Hour mark position (%)"),
                               cfg.markPosition, kMarkScaleMin, kMarkScaleMax, true);
    m_minuteMarkScale = addSlider(sizesGrid, srow++, QStringLiteral("Minute mark size (%)"),
                                  cfg.minuteMarkScale, kMarkScaleMin, kMarkScaleMax, true);
    m_minuteMarkScale->setToolTip(
        QStringLiteral("Percentage of the hour mark size. 0 hides the minute marks."));
    grid->addWidget(sizesBox, row, 0, 1, 4);
    ++row;

    // ------------------------------------------------------- opacity section
    //
    // Each part of the drawing fades on its own, so a face can wash out to
    // bare wire over the wallpaper while the hands stay solid.  The two sync
    // boxes cover the common case of wanting a pair to move together.
    auto *opacityBox = new QGroupBox(QStringLiteral("Opacity"), this);
    auto *opacityGrid = new QGridLayout(opacityBox);
    opacityGrid->setHorizontalSpacing(10);
    opacityGrid->setVerticalSpacing(8);
    int orow = 0;

    m_faceOpacity = addSlider(opacityGrid, orow++, QStringLiteral("Face (%)"), cfg.faceOpacity,
                              kOpacityMin, kOpacityMax, false);
    m_faceOpacity->setToolTip(QStringLiteral(
        "The body of the artwork. At 0 only the wire is left, and the desktop shows "
        "through the face."));
    m_wireOpacity = addSlider(opacityGrid, orow++, QStringLiteral("Wire (%)"), cfg.wireOpacity,
                              kOpacityMin, kOpacityMax, false);
    m_wireOpacity->setToolTip(
        QStringLiteral("The line work of the artwork -- its outlines and shading."));
    m_syncFaceWire = addSyncBox(opacityGrid, orow++, QStringLiteral("sync face/wire"),
                                cfg.syncFaceWire, m_faceOpacity, m_wireOpacity);

    m_handOpacity = addSlider(opacityGrid, orow++, QStringLiteral("Clock hands (%)"),
                              cfg.handOpacity, kOpacityMin, kOpacityMax, false);
    m_handOpacity->setToolTip(
        QStringLiteral("The hour, minute and second hands, and the pin they turn on."));
    m_markOpacity = addSlider(opacityGrid, orow++, QStringLiteral("Clock marks (%)"),
                              cfg.markOpacity, kOpacityMin, kOpacityMax, false);
    m_markOpacity->setToolTip(QStringLiteral("The hour and minute indices around the dial."));
    m_syncHandsMarks = addSyncBox(opacityGrid, orow++, QStringLiteral("sync hands/marks"),
                                  cfg.syncHandsMarks, m_handOpacity, m_markOpacity);

    grid->addWidget(opacityBox, row, 0, 1, 4);
    ++row;

    // The three options are each about one of the sections below, so each one
    // now heads the section it governs rather than sitting in a row of its own.
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

    // --------------------------------------------------------------- colours
    // Three groups, because these are three separate jobs: what the hands look
    // like, what the dial's markings look like, and what the artwork behind
    // them looks like.  Ungrouped it was one undifferentiated run of swatches.
    auto *handsBox = new QGroupBox(QStringLiteral("Hands"), this);
    auto *handsGrid = new QGridLayout(handsBox);
    handsGrid->setHorizontalSpacing(10);
    int hrow = 0;

    handsGrid->addWidget(m_smoothSweep, hrow, 0, 1, 2);
    ++hrow;
    handsGrid->addWidget(m_reverseTime, hrow, 0, 1, 2);
    ++hrow;

    addLabel(handsGrid, QStringLiteral("Second"), hrow);
    m_second = new ColorButton(QColor(cfg.secondColor), false,
                               QStringLiteral("Second hand color"), this);
    // Untick to drop the second hand entirely.  The colour stays put while it
    // is off, so ticking it back on returns the hand you had.
    m_secondShown = new QCheckBox(QStringLiteral("enabled"), this);
    m_secondShown->setChecked(cfg.showSecond);
    m_secondShown->setToolTip(QStringLiteral("Draw the second hand"));
    handsGrid->addLayout(withOption(m_second, m_secondShown), hrow, 1);
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
    addLabel(handsGrid, QStringLiteral("Location"), hrow);
    m_pick = new QPushButton(QStringLiteral("Pick on clock\u2026"), this);
    m_pick->setToolTip(QStringLiteral(
        "Drag on the clock face, or use the arrow keys to move the pivot a pixel "
        "at a time (hold Shift for 10). Enter accepts, Esc cancels."));
    connect(m_pick, &QPushButton::clicked, this, [this] { m_clock->startPicking(); });
    m_centerAuto = new QCheckBox(QStringLiteral("center on image"), this);
    m_centerAuto->setChecked(!m_clock->cfg().center.has_value());
    connect(m_centerAuto, &QCheckBox::toggled, this, [this](bool on) {
        if (on) {
            m_clock->stopPicking();
            m_clock->setCenter(std::nullopt, true);
        }
        refreshCenter();
    });
    handsGrid->addWidget(m_pick, hrow, 1, Qt::AlignLeft);
    ++hrow;
    handsGrid->addWidget(m_centerAuto, hrow, 1);
    ++hrow;

    addLabel(handsGrid, QStringLiteral("Current location:"), hrow);
    m_centerLabel = new QLabel(this);
    m_centerLabel->setTextFormat(Qt::RichText);
    handsGrid->addWidget(m_centerLabel, hrow, 1);
    ++hrow;
    handsGrid->setRowStretch(hrow, 1);

    auto *marksBox = new QGroupBox(QStringLiteral("Marks"), this);
    auto *marksGrid = new QGridLayout(marksBox);
    marksGrid->setHorizontalSpacing(10);
    int mrow = 0;

    marksGrid->addWidget(m_quarterMarks, mrow, 0, 1, 2);
    ++mrow;

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
    // useAlpha lets the swatch show the checkerboard when the face is faded.
    m_face = new ColorButton(QColor(cfg.faceColor), true, QStringLiteral("Face color"), this);
    faceGrid->addWidget(m_face, frow, 1, Qt::AlignLeft);
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
    QPushButton *cancel = buttons->addButton(QStringLiteral("Cancel"),
                                             QDialogButtonBox::RejectRole);
    save->setIcon(glyphIcon(Glyph::Save, GlyphRole::Go));
    save->setIconSize(QSize(18, 18));
    cancel->setIcon(glyphIcon(Glyph::Cancel, GlyphRole::Stop));
    cancel->setIconSize(QSize(18, 18));
    save->setDefault(true);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    outer->addWidget(buttons);

    // ------------------------------------------------------------- wiring up
    for (QSlider *slider : {m_size, m_handScale, m_markScale, m_markPosition,
                            m_minuteMarkScale, m_faceOpacity, m_wireOpacity,
                            m_handOpacity, m_markOpacity}) {
        connect(slider, &QSlider::valueChanged, this, [this] { onChanged(); });
    }
    for (ColorButton *button : {m_second, m_hour, m_minute, m_face, m_wire, m_hourMark,
                                m_minuteMark}) {
        connect(button, &ColorButton::colorSet, this, [this, button] { onChanged(button); });
    }
    for (QCheckBox *box : {m_minuteSame, m_secondShown, m_quarterMarks, m_smoothSweep,
                           m_reverseTime, m_syncFaceWire, m_syncHandsMarks}) {
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

// Keeps a pair of sliders on one value while the box is ticked.  Ticking it
// pulls the second slider onto the first rather than averaging or leaving them
// apart, so the tick has a visible, predictable effect: the value you were
// last looking at wins.
//
// The two connections are plain setValue() calls, which emit nothing when the
// value is already right, so the pair settles instead of ping-ponging.
QCheckBox *SettingsDialog::addSyncBox(QGridLayout *grid, int row, const QString &caption,
                                      bool checked, QSlider *first, QSlider *second)
{
    auto *box = new QCheckBox(caption, this);
    box->setChecked(checked);
    if (checked)
        second->setValue(first->value());

    const auto follow = [box](QSlider *from, QSlider *to) {
        connect(from, &QSlider::valueChanged, to, [box, to](int value) {
            if (box->isChecked())
                to->setValue(value);
        });
    };
    follow(first, second);
    follow(second, first);
    connect(box, &QCheckBox::toggled, second, [first, second](bool on) {
        if (on)
            second->setValue(first->value());
    });

    // Under the sliders and hard against them, in the same column, so it reads
    // as belonging to the pair above rather than to the group as a whole.
    grid->addWidget(box, row, 1, Qt::AlignLeft);
    return box;
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

    QSpinBox *readout = makeReadout(low, high);
    readout->setValue(slider->value());
    // Each follows the other.  setValue() on a control already holding that
    // value emits nothing, so the pair settles rather than looping.
    connect(slider, &QSlider::valueChanged, readout, &QSpinBox::setValue);
    connect(readout, &QSpinBox::valueChanged, slider, &QSlider::setValue);

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
    // The sync boxes go first: with one ticked, setting either slider of the
    // pair carries the other with it, which is what a preset wants anyway.
    m_syncFaceWire->setChecked(values.syncFaceWire);
    m_syncHandsMarks->setChecked(values.syncHandsMarks);
    m_faceOpacity->setValue(values.faceOpacity);
    m_wireOpacity->setValue(values.wireOpacity);
    m_handOpacity->setValue(values.handOpacity);
    m_markOpacity->setValue(values.markOpacity);
    m_quarterMarks->setChecked(values.quarterMarksOnly);
    m_minuteSame->setChecked(values.minuteSameAsHour);
    m_secondShown->setChecked(values.showSecond);
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
    // The swatch shows the face's opacity in its alpha, so only the colour
    // itself is taken back from it -- hexOf() drops the alpha for us.
    if (sender == m_face)
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

    // A hand that is not drawn has no colour worth setting.  The value is kept,
    // so ticking it back on returns the colour that was there.
    m_second->setEnabled(m_secondShown->isChecked());

    // The face and wire colours are the recolour's two ends, so they mean
    // nothing at all when the artwork is drawn as authored.
    const bool recolor = recolorMode();
    m_face->setEnabled(recolor);
    m_wire->setEnabled(recolor);
    // The swatch carries the face's own opacity, so a face faded to nothing
    // reads as the checkerboard rather than as a colour that does not show.
    QColor faceColor(m_faceOwn);
    if (!faceColor.isValid())
        faceColor = QColor(Qt::white);
    faceColor.setAlpha(qBound(0, m_faceOpacity->value(), 100) * 255 / 100);
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
    // Only one direction of this box does anything. Ticked, it is already the
    // state we are in; unticking it asks for a manual pivot without saying
    // where, so it used to snap straight back. Disabled while it is on, it
    // stops offering a move it cannot make, and stays live while a manual
    // pivot is set, which is the way back.
    m_centerAuto->setEnabled(!autoCenter);
    m_centerAuto->setToolTip(autoCenter
                                 ? QStringLiteral("The hands turn on the centre of the image. "
                                                  "Use Pick on clock to put them elsewhere.")
                                 : QStringLiteral("Put the hands back on the centre of the "
                                                  "image."));
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
    out.faceOpacity = m_faceOpacity->value();
    out.wireOpacity = m_wireOpacity->value();
    out.syncFaceWire = m_syncFaceWire->isChecked();
    out.handOpacity = m_handOpacity->value();
    out.markOpacity = m_markOpacity->value();
    out.syncHandsMarks = m_syncHandsMarks->isChecked();
    out.quarterMarksOnly = m_quarterMarks->isChecked();
    out.faceSvg = faceSvg();
    out.faceDefault = faceDefault();
    out.secondColor = hexOf(m_second->color());
    out.hourColor = hexOf(m_hour->color());
    out.minuteColor = m_minuteOwn;
    out.minuteSameAsHour = m_minuteSame->isChecked();
    out.showSecond = m_secondShown->isChecked();
    out.smoothSweep = m_smoothSweep->isChecked();
    out.reverseTime = m_reverseTime->isChecked();
    out.faceColor = m_faceOwn;
    out.faceRecolor = recolorMode();
    out.wireColor = hexOf(m_wire->color());
    out.hourMarkColor = hexOf(m_hourMark->color());
    out.minuteMarkColor = hexOf(m_minuteMark->color());
    return out;
}
