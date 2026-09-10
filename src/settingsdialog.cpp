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
#include <QMessageBox>
#include <QPushButton>
#include <QRandomGenerator>
#include <QRadioButton>
#include <QScreen>
#include <QScrollArea>
#include <QScrollBar>
#include <QSlider>
#include <QSpinBox>
#include <QTabWidget>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <limits>

namespace {

// How many preset thumbnails to a row.  They wrap rather than running on, so
// that adding another preset makes the box one row taller instead of making
// the whole dialog wider than the tabs beneath it.
constexpr int kPresetColumns = 6;

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

// Put the middle item of a three-part row in the centre of the row rather than
// in the centre of the gap between its neighbours.  A stretch either side only
// shares out what is left over, so the middle lands off-centre by half the
// difference between the outer two; padding the narrower of them until they
// match makes the two shares equal and the middle truly central.
void centreMiddleItem(QHBoxLayout *row, QWidget *left, QWidget *right)
{
    left->ensurePolished();
    right->ensurePolished();
    const int leftWidth = left->sizeHint().width();
    const int rightWidth = right->sizeHint().width();
    if (leftWidth == rightWidth)
        return;
    if (leftWidth < rightWidth)
        row->insertSpacing(row->indexOf(left), rightWidth - leftWidth);
    else
        row->addSpacing(leftWidth - rightWidth);
}

// Tie an "enabled" box to the size slider that decides whether the thing is
// drawn at all.  Two views of one setting, so the binding runs both ways, and
// the size the slider held when it was turned off is kept so that turning it
// back on returns what was there rather than a default.
void bindShown(QCheckBox *box, QSlider *slider, int &remembered)
{
    remembered = slider->value() > 0 ? slider->value() : 100;
    box->setChecked(slider->value() > 0);
    QObject::connect(box, &QCheckBox::toggled, slider, [box, slider, &remembered](bool on) {
        if (on == (slider->value() > 0))
            return;
        const QSignalBlocker blockBox(box);  // the slider will tick it for us
        slider->setValue(on ? remembered : 0);
    });
    QObject::connect(slider, &QSlider::valueChanged, box, [box, &remembered](int value) {
        if (value > 0)
            remembered = value;
        const QSignalBlocker block(box);  // setting the box back would fight the slider
        box->setChecked(value > 0);
    });
}


}  // namespace

SettingsDialog::SettingsDialog(ClockWindow *clock)
    : QDialog(clock, Qt::Dialog)
    , m_clock(clock)
{
    // Not modal, so the clock stays interactive and the dialog can be moved
    // around freely while previewing changes.
    refreshTitle();
    setModal(false);
    setWindowFlag(Qt::WindowStaysOnTopHint, false);

    const Config &cfg = m_clock->cfg();
    m_opened = cfg;
    m_minuteOwn = cfg.minuteColor;
    m_faceOwn = cfg.faceColor;

    auto *outer = new QVBoxLayout(this);

    // Every control lives on this widget, which the scroll area below pans over
    // when the dialog is taller or wider than the screen it opens on.
    auto *content = new QWidget;
    auto *column = new QVBoxLayout(content);
    column->setSpacing(10);
    column->setContentsMargins(12, 12, 12, 12);

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

    // ------------------------------------------------------------- presets
    //
    // Above the tabs rather than on one of them, because a preset writes to
    // every tab at once: choosing one changes hand colours, mark sizes and
    // opacity together.  On a tab of its own the effect would be to alter
    // pages the user cannot see, which is the one thing tabs are bad at.
    auto *presetsBox = new QGroupBox(QStringLiteral("Presets"), this);
    auto *presetsLayout = new QGridLayout(presetsBox);
    presetsLayout->setHorizontalSpacing(6);
    presetsLayout->setVerticalSpacing(6);
    const qreal dpr = devicePixelRatioF();
    int pcol = 0, prow = 0;
    for (const Preset &preset : presets()) {
        auto *button = new QToolButton(this);
        button->setIcon(QIcon(presetThumbnail(preset.values, kPresetThumb, dpr)));
        button->setIconSize(QSize(kPresetThumb, kPresetThumb));
        button->setAutoRaise(false);
        button->setToolTip(preset.name + QStringLiteral(" \u2014 ") + preset.tip);
        connect(button, &QToolButton::clicked, this,
                [this, &preset, button] { onPresetClicked(preset, button); });
        presetsLayout->addWidget(button, prow, pcol);
        if (++pcol == kPresetColumns) {
            pcol = 0;
            ++prow;
        }
    }
    // The trailing column takes the slack, so a part-filled last row stays left
    // aligned under the one above instead of spreading out to fill the width.
    presetsLayout->setColumnStretch(kPresetColumns, 1);
    column->addWidget(presetsBox);

    // The tabs.  Each page below is one of the groups this dialog used to
    // stack vertically; the page's tab carries the name the group box did.
    auto *tabs = new QTabWidget(this);
    column->addWidget(tabs, 1);

    // --------------------------------------------------------- face tab
    // The artwork and the two colours it is drawn in, since the colouring mode
    // decides whether those colours apply at all.
    auto *chooserBox = new QWidget;
    auto *chooserGrid = new QGridLayout(chooserBox);
    chooserGrid->setHorizontalSpacing(10);
    int crow = 0;

    // Size heads the page.  It is the one setting that is about the clock
    // rather than about any part of it, and it is the one reached most often,
    // so it goes where the dialog opens rather than on a page of its own.
    const int maxSize = m_clock->maxSize();
    m_size = addSlider(chooserGrid, crow++, QStringLiteral("Clock size (px)"), cfg.size,
                       kSizeMin, maxSize, false);

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

    // The mode governs the two swatches under it, so it sits with them.
    addLabel(chooserGrid, QStringLiteral("Coloring"), crow);
    m_colorMode = new QComboBox(this);
    m_colorMode->addItem(QStringLiteral("Recolor"), true);
    m_colorMode->addItem(QStringLiteral("Original"), false);
    m_colorMode->setCurrentIndex(cfg.faceRecolor ? 0 : 1);
    m_wasRecolor = cfg.faceRecolor;
    m_colorMode->setToolTip(QStringLiteral(
        "Recolor maps the artwork's shading onto the face and wire colors below, so "
        "the drawing comes out in your colors with its shading intact.\n\n"
        "Original draws the file exactly as it was authored, which suits a picture "
        "that already has colors of its own. The face and wire colors then do not "
        "apply; the hands and marks still do."));
    chooserGrid->addWidget(m_colorMode, crow, 1, Qt::AlignLeft);
    ++crow;

    addLabel(chooserGrid, QStringLiteral("Face color"), crow);
    // useAlpha lets the swatch show the checkerboard when the face is faded.
    m_face = new ColorButton(QColor(cfg.faceColor), true, QStringLiteral("Face color"), this);
    m_faceRandom = new QCheckBox(QStringLiteral("random"), this);
    m_faceRandom->setChecked(cfg.faceColorRandom);
    m_faceRandom->setToolTip(QStringLiteral(
        "Pick this color by chance instead of choosing it. The color it lands on goes "
        "into the swatch, so what is on screen is always what is in the box.\n\n"
        "On the kaleidoscope it means a little more: rolled, the face is a scheme of "
        "several hues built around the color, and chosen, it is that one color and its "
        "shades."));
    m_faceCycle = new QPushButton(QStringLiteral("Cycle"), this);
    m_faceCycle->setToolTip(QStringLiteral("Roll another color"));
    chooserGrid->addLayout(withOption(m_face, m_faceRandom, m_faceCycle), crow, 1);
    ++crow;

    addLabel(chooserGrid, QStringLiteral("Wire color"), crow);
    m_wire = new ColorButton(QColor(cfg.wireColor), false, QStringLiteral("Wire color"), this);
    m_wireRandom = new QCheckBox(QStringLiteral("random"), this);
    m_wireRandom->setChecked(cfg.wireColorRandom);
    m_wireRandom->setToolTip(QStringLiteral(
        "Pick the line color by chance instead of choosing it. Line work has to read as "
        "line work, so the roll stays near one end of the tone range rather than "
        "wandering into the middle."));
    m_wireCycle = new QPushButton(QStringLiteral("Cycle"), this);
    m_wireCycle->setToolTip(QStringLiteral("Roll another color"));
    chooserGrid->addLayout(withOption(m_wire, m_wireRandom, m_wireCycle), crow, 1);
    ++crow;

    // A face costs nothing to ask for again, so it can be left to redraw
    // itself on a timer rather than waiting for anyone to click.
    addLabel(chooserGrid, QStringLiteral("Regenerate"), crow);
    m_regen = new QCheckBox(QStringLiteral("every"), this);
    m_regen->setChecked(cfg.faceRegen);
    m_regen->setToolTip(QStringLiteral(
        "Redraw the face every so often, exactly as clicking its preset again does: "
        "fresh colors wherever the color above is marked as random, and on the "
        "kaleidoscope a new pattern as well.\n\n"
        "One is drawn at startup too, so a clock left running is never showing the same "
        "face it was shut down with."));
    m_regenMinutes = new QSpinBox(this);
    m_regenMinutes->setRange(kRegenMinutesMin, kRegenMinutesMax);
    m_regenMinutes->setValue(cfg.faceRegenMinutes);
    m_regenMinutes->setSuffix(QStringLiteral(" min"));
    m_regenMinutes->setToolTip(QStringLiteral("How long each face is kept, in minutes"));
    auto *regenRow = new QHBoxLayout;
    regenRow->setContentsMargins(0, 0, 0, 0);
    regenRow->setSpacing(8);
    regenRow->addWidget(m_regen);
    regenRow->addWidget(m_regenMinutes);
    regenRow->addStretch(1);
    chooserGrid->addLayout(regenRow, crow, 1);
    ++crow;

    chooserGrid->setColumnStretch(1, 1);
    chooserGrid->setRowStretch(crow, 1);

    // --------------------------------------------------------- opacity tab
    //
    // Each part of the drawing fades on its own, so a face can wash out to
    // bare wire over the wallpaper while the hands stay solid.  The two sync
    // boxes cover the common case of wanting a pair to move together, and are
    // why the four sliders stay on one page rather than following their parts
    // to the Hands and Marks tabs: a box that ties together two controls the
    // user cannot see at once would be a puzzle rather than a convenience.
    auto *opacityBox = new QWidget;
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

    opacityGrid->setRowStretch(orow, 1);

    // Each of these three heads the tab it governs rather than sitting in a
    // row of its own.
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

    // --------------------------------------------------------- hands tab
    auto *handsBox = new QWidget;
    auto *handsGrid = new QGridLayout(handsBox);
    handsGrid->setHorizontalSpacing(10);
    int hrow = 0;

    // The two switches share a row.  Neither has a long label and neither
    // needs a column of its own, and pairing them buys the page the height the
    // hand size slider needs.
    auto *handSwitches = new QHBoxLayout;
    handSwitches->setSpacing(12);
    handSwitches->addWidget(m_smoothSweep);
    handSwitches->addWidget(m_reverseTime);
    handSwitches->addStretch(1);
    handsGrid->addLayout(handSwitches, hrow, 0, 1, 2);
    ++hrow;

    m_handScale = addSlider(handsGrid, hrow++, QStringLiteral("Hand size (%)"), cfg.handScale,
                            kHandScaleMin, kHandScaleMax, true);

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

    // --------------------------------------------------------- marks tab
    auto *marksBox = new QWidget;
    auto *marksGrid = new QGridLayout(marksBox);
    marksGrid->setHorizontalSpacing(10);
    int mrow = 0;

    marksGrid->addWidget(m_quarterMarks, mrow, 0, 1, 2);
    ++mrow;

    // Each kind of mark keeps its own block -- colour, then the sliders that
    // size and place it -- rather than the colours sitting together on one tab
    // and the sizes together on another.  Someone adjusting the hour marks
    // wants everything about them within reach.
    addLabel(marksGrid, QStringLiteral("Hour"), mrow);
    m_hourMark = new ColorButton(QColor(cfg.hourMarkColor), false,
                                 QStringLiteral("Hour mark color"), this);
    // Untick to drop the hour marks: the size goes to zero and the size it had
    // is remembered, so ticking the box back on returns the marks you had
    // rather than some default.  Zeroing the slider by hand ticks the box off
    // for the same reason -- the two are two views of one setting.
    m_hourMarkShown = new QCheckBox(QStringLiteral("enabled"), this);
    m_hourMarkShown->setToolTip(QStringLiteral("Draw the hour marks"));
    marksGrid->addLayout(withOption(m_hourMark, m_hourMarkShown), mrow, 1);
    ++mrow;

    m_markScale = addSlider(marksGrid, mrow++, QStringLiteral("Hour mark size (%)"),
                            cfg.markScale, kMarkScaleMin, kMarkScaleMax, true);
    m_markPosition = addSlider(marksGrid, mrow++, QStringLiteral("Hour mark position (%)"),
                               cfg.markPosition, kMarkScaleMin, kMarkScaleMax, true);

    addLabel(marksGrid, QStringLiteral("Minute"), mrow);
    m_minuteMark = new ColorButton(QColor(cfg.minuteMarkColor), false,
                                   QStringLiteral("Minute mark color"), this);
    m_minuteMarkShown = new QCheckBox(QStringLiteral("enabled"), this);
    m_minuteMarkShown->setToolTip(QStringLiteral("Draw the minute marks"));
    marksGrid->addLayout(withOption(m_minuteMark, m_minuteMarkShown), mrow, 1);
    ++mrow;

    m_minuteMarkScale = addSlider(marksGrid, mrow++, QStringLiteral("Minute mark size (%)"),
                                  cfg.minuteMarkScale, kMarkScaleMin, kMarkScaleMax, true);
    m_minuteMarkScale->setToolTip(
        QStringLiteral("Percentage of the hour mark size. 0 hides the minute marks."));

    bindShown(m_hourMarkShown, m_markScale, m_hourMarkLast);
    bindShown(m_minuteMarkShown, m_minuteMarkScale, m_minuteMarkLast);

    marksGrid->setRowStretch(mrow, 1);

    // The tab order, in one place.  The three parts of the clock first, in the
    // order they are drawn -- face behind, then its marks, then the hands over
    // both -- and after them the page that cuts across all three.
    tabs->addTab(chooserBox, QStringLiteral("Face"));
    tabs->addTab(marksBox, QStringLiteral("Marks"));
    tabs->addTab(handsBox, QStringLiteral("Hands"));
    tabs->addTab(opacityBox, QStringLiteral("Opacity"));

    // --------------------------------------------------------------- buttons
    // Save and Cancel go in a button box so the platform puts them in its own
    // order; the other two are placed by hand, since a button box would gather
    // them into the same cluster and they are not part of that decision.
    auto *buttons = new QDialogButtonBox(this);

    // Settings is otherwise a dead end: it governs this clock alone, and while
    // it has focus the Ctrl+K that would reach Manage clocks goes to the dialog
    // rather than to the clock behind it.  So the one route from the part to
    // the whole is a button on the part.  It is kept away from Save and Cancel
    // -- it commits nothing, and a third button in that cluster would read as
    // though it did.
    auto *manage = new QPushButton(QStringLiteral("Manage clocks..."));
    manage->setIcon(glyphIcon(Glyph::List, GlyphRole::Info));
    manage->setIconSize(QSize(18, 18));
    manage->setAutoDefault(false);
    connect(manage, &QPushButton::clicked, this, [this] {
        if (m_clock)
            m_clock->manageClocks();
    });

    // Reset throws away every setting in the dialog, so it is kept as far from
    // Save as the row allows rather than sitting beside it.
    auto *reset = new QPushButton(QStringLiteral("Reset"));
    reset->setIcon(glyphIcon(Glyph::Reset, GlyphRole::Warn));
    reset->setIconSize(QSize(18, 18));
    reset->setAutoDefault(false);
    connect(reset, &QPushButton::clicked, this, &SettingsDialog::onResetClicked);

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

    // Reset is meant to sit in the middle of the row, so the space either side
    // of it is shared out to make it so however wide its neighbours are: a pair
    // of stretches alone would only centre it between them.
    auto *bottom = new QWidget(this);
    auto *bottomRow = new QHBoxLayout(bottom);
    bottomRow->setContentsMargins(0, 0, 0, 0);
    bottomRow->addWidget(manage);
    bottomRow->addStretch(1);
    bottomRow->addWidget(reset);
    bottomRow->addStretch(1);
    bottomRow->addWidget(buttons);
    centreMiddleItem(bottomRow, manage, buttons);
    outer->addWidget(bottom);

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
                           m_reverseTime, m_syncFaceWire, m_syncHandsMarks, m_regen}) {
        connect(box, &QCheckBox::toggled, this, [this] { onChanged(); });
    }
    connect(m_regenMinutes, &QSpinBox::valueChanged, this, [this] { onChanged(); });

    // Ticking "random" is itself a request for a colour, so it rolls one
    // rather than only changing what the dialog will do next; Cycle then rolls
    // another.  Unticking leaves the colour where it is, so the roll you
    // happened to like is the one you keep and can then adjust.
    connect(m_faceRandom, &QCheckBox::toggled, this, [this](bool on) {
        if (on)
            rollColor(true);
        else
            onChanged();
    });
    connect(m_wireRandom, &QCheckBox::toggled, this, [this](bool on) {
        if (on)
            rollColor(false);
        else
            onChanged();
    });
    connect(m_faceCycle, &QPushButton::clicked, this, [this] { rollColor(true); });
    connect(m_wireCycle, &QPushButton::clicked, this, [this] { rollColor(false); });
    connect(m_colorMode, &QComboBox::currentIndexChanged, this, [this] {
        // Leaving Recolor for Original on a generated face changes what the two
        // colours are for: under Recolor they are the ends of a ramp laid over
        // grey artwork, and under Original they are what the drawing is built
        // out of.  A pair picked to make a good ramp is not a pair anyone chose
        // to be drawn in, so the switch rolls a fresh pair rather than carrying
        // the old one over into a job it was never meant for.
        const bool generated =
            faceSvg().startsWith(kBuiltinFacePrefix + kKaleidoscopeFace + QLatin1Char(':'));
        if (m_live && generated && m_wasRecolor && !recolorMode()) {
            const QSignalBlocker faceBlock(m_faceRandom);
            const QSignalBlocker wireBlock(m_wireRandom);
            m_faceRandom->setChecked(true);
            m_wireRandom->setChecked(true);
            m_faceOwn = hexOf(rollAgainst(true, m_wire->color()));
            m_wire->setColor(rollAgainst(false, QColor(m_faceOwn)));
        }
        m_wasRecolor = recolorMode();
        onChanged();
    });

    syncSwatches();
    refreshCenter();
    m_live = true;

    resizeToFit(content, scroll, bottom);
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
QHBoxLayout *SettingsDialog::withOption(QWidget *control, QWidget *box, QWidget *extra)
{
    auto *layout = new QHBoxLayout;
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);
    layout->addWidget(control, 0);
    layout->addWidget(box, 0);
    if (extra)
        layout->addWidget(extra, 0);
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

void SettingsDialog::onPresetClicked(const Preset &preset, QToolButton *button)
{
    const QString kaleido = kBuiltinFacePrefix + kKaleidoscopeFace + QLatin1Char(':');
    const bool wasKaleido = faceSvg().startsWith(kaleido);

    Config values = m_clock->cfg();
    copyPresetKeys(preset.values, values);

    // The kaleidoscope is a different face every time it is asked for, so its
    // button is a reroll rather than a fixed choice: each click picks a new
    // seed.  The button's own thumbnail is redrawn to match, because a preset
    // button that shows a picture other than the one it just applied would be
    // telling the user something untrue.
    if (values.faceSvg.startsWith(kaleido)) {
        values.faceSvg =
            kaleido + QString::number(QRandomGenerator::global()->generate64());
        // Coming from a kaleidoscope, the colours on screen are the ones to
        // start from, so a colour chosen by hand survives a reroll.  Coming
        // from another face the preset's own colours are the starting point --
        // though they are only what its thumbnail is drawn in, and taking them
        // as they stand would make every kaleidoscope in the world the same
        // blue.
        if (wasKaleido) {
            values.faceColorRandom = m_faceRandom->isChecked();
            values.wireColorRandom = m_wireRandom->isChecked();
            values.faceColor = m_faceOwn;
            values.wireColor = hexOf(m_wire->color());
        }
        // Either way, a colour marked as rolled is rolled again.  That is what
        // the mark means, and asking for another kaleidoscope and getting the
        // old line colour back looks like the button half missed.
        if (values.faceColorRandom)
            values.faceColor = hexOf(rollAgainst(true, QColor(values.wireColor)));
        if (values.wireColorRandom)
            values.wireColor = hexOf(rollAgainst(false, QColor(values.faceColor)));
        if (button)
            button->setIcon(QIcon(presetThumbnail(values, kPresetThumb, devicePixelRatioF())));
    }

    applyValues(values, false);
}

// Reset here has two meanings worth telling apart: undoing this sitting's
// changes, which is what someone who has just made a mess of the sliders wants,
// and going back to the defaults.  The menu's Reset only offers the second,
// there being no sitting to undo.
SettingsDialog::ResetTo SettingsDialog::askReset()
{
    QMessageBox box(this);
    box.setWindowTitle(QStringLiteral("vclock"));
    box.setIcon(QMessageBox::Question);
    box.setText(QStringLiteral("Reset this clock?"));

    auto *undo = new QRadioButton(QStringLiteral("Undo my changes"), &box);
    auto *factory = new QRadioButton(QStringLiteral("Restore the defaults"), &box);
    undo->setChecked(true);

    // A message box lays itself out in a grid, with the icon in the first
    // column and the buttons on the last row.  A layout can only be added to
    // the end of a grid, so the buttons come out and go back on after, which
    // puts the choice between the question and them where it belongs.
    if (auto *grid = qobject_cast<QGridLayout *>(box.layout())) {
        auto *buttons = box.findChild<QDialogButtonBox *>();
        if (buttons)
            grid->removeWidget(buttons);
        auto *choice = new QVBoxLayout;
        choice->setContentsMargins(0, 6, 0, 0);
        choice->addWidget(undo);
        choice->addWidget(factory);
        grid->addLayout(choice, grid->rowCount(), 1, 1, grid->columnCount() - 1);
        if (buttons)
            grid->addWidget(buttons, grid->rowCount(), 0, 1, grid->columnCount());
    }

    box.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
    box.setDefaultButton(QMessageBox::Cancel);
    if (box.exec() != QMessageBox::Ok)
        return ResetTo::Cancelled;
    return undo->isChecked() ? ResetTo::Opened : ResetTo::Factory;
}

void SettingsDialog::onResetClicked()
{
    if (!m_clock)
        return;
    const ResetTo to = askReset();
    if (to == ResetTo::Cancelled)
        return;
    // The controls go back and preview like any other change, so Save commits
    // the reset and Cancel puts the clock back as it was.  The menu's Reset
    // defaults saves at once because there is no dialog there to take the
    // decision.
    applyValues(to == ResetTo::Opened ? m_opened : m_clock->defaultConfig(), true);
}

// Snap every appearance control to a set of values, then preview it once.  A
// preset is a change of looks and leaves the size and the running behaviour
// alone; a reset restores those too, which is what "full" means here.
void SettingsDialog::applyValues(const Config &values, bool full)
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

    if (full) {
        m_size->setValue(values.size);
        m_smoothSweep->setChecked(values.smoothSweep);
        m_reverseTime->setChecked(values.reverseTime);
    }

    // A preset is a change of looks, so it leaves the clock's size alone; it
    // does restore the hand pivot, which is part of the drawing.  The centre
    // lives on the clock rather than in a widget, so it has to be set there;
    // refreshCenter() then re-syncs the "auto" checkbox.
    m_clock->stopPicking();
    m_clock->setCenter(values.center, false);

    m_handScale->setValue(values.handScale);
    m_markScale->setValue(values.markScale);
    m_markPosition->setValue(values.markPosition);
    m_minuteMarkScale->setValue(values.minuteMarkScale);
    // The sync boxes go first: with one ticked, setting either slider of the
    // pair carries the other with it, which is what a preset wants anyway.
    m_faceRandom->setChecked(values.faceColorRandom);
    m_wireRandom->setChecked(values.wireColorRandom);
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
    m_regen->setChecked(values.faceRegen);
    m_regenMinutes->setValue(values.faceRegenMinutes);

    m_faceOwn = values.faceColor;
    m_minuteOwn = values.minuteColor;
    m_second->setColor(QColor(values.secondColor));
    m_hour->setColor(QColor(values.hourColor));
    m_wire->setColor(QColor(values.wireColor));
    m_hourMark->setColor(QColor(values.hourMarkColor));
    m_minuteMark->setColor(QColor(values.minuteMarkColor));

    m_live = wasLive;
    m_wasRecolor = values.faceRecolor;
    onChanged();
}

// Roll one of the two face colours and put it where it can be seen.  The
// swatch is the record of what was rolled -- there is no separate memory of
// it -- so a rolled colour is a colour like any other and can be nudged by
// hand afterwards.
void SettingsDialog::rollColor(bool faceEnd)
{
    if (faceEnd)
        m_faceOwn = hexOf(rollAgainst(true, m_wire->color()));
    else
        m_wire->setColor(rollAgainst(false, QColor(m_faceOwn)));
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

    // Picking a colour by hand answers the same question the tick does, and
    // answers it the other way: this one was chosen, not rolled.  Only a
    // colour the user set counts, which is what makes the sender worth
    // testing -- setColor() is silent, so a rolled colour arrives here with no
    // sender at all and leaves the tick alone.  The signal is blocked because
    // the apply below already covers the change; letting the box emit would
    // run the whole of onChanged() a second time.
    if (sender == m_face && m_faceRandom->isChecked()) {
        const QSignalBlocker block(m_faceRandom);
        m_faceRandom->setChecked(false);
    }
    if (sender == m_wire && m_wireRandom->isChecked()) {
        const QSignalBlocker block(m_wireRandom);
        m_wireRandom->setChecked(false);
    }

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
    // nothing at all when a drawn face is left as it was authored.  A
    // generated face is the exception: it is built from the two colours rather
    // than repainted with them, so they matter to it whatever the mode says.
    //
    // Whether a colour was rolled or chosen makes no difference to that, so
    // the ticks and their Cycle buttons follow the swatches: live wherever the
    // colour beside them is live.
    const bool live = recolorMode() || faceSvg().startsWith(kBuiltinFacePrefix
                                                            + kKaleidoscopeFace
                                                            + QLatin1Char(':'));
    m_face->setEnabled(live);
    m_wire->setEnabled(live);
    m_faceRandom->setEnabled(live);
    m_wireRandom->setEnabled(live);
    m_faceCycle->setEnabled(live && m_faceRandom->isChecked());

    // Regenerating is asking for the same things again that the two rows above
    // hold, so it is live in exactly the cases they are.  Where they are dead
    // -- a drawn face left as its author coloured it -- there is nothing for a
    // timer to change, and an enabled box that did nothing would say otherwise.
    m_regen->setEnabled(live);
    m_regenMinutes->setEnabled(live && m_regen->isChecked());
    m_wireCycle->setEnabled(live && m_wireRandom->isChecked());
    // The swatch carries the face's own opacity, so a face faded to nothing
    // reads as the checkerboard rather than as a colour that does not show.
    QColor faceColor(m_faceOwn);
    if (!faceColor.isValid())
        faceColor = QColor(Qt::white);
    faceColor.setAlpha(qBound(0, m_faceOpacity->value(), 100) * 255 / 100);
    m_face->setColor(faceColor);
}

void SettingsDialog::refreshTitle()
{
    const QString which = m_clock->configName();
    setWindowTitle(which.isEmpty() ? QStringLiteral("Clock Settings")
                                   : QStringLiteral("Clock Settings \u2014 ") + which);
}

void SettingsDialog::nudgeSize(int steps, bool fine)
{
    // A step is a fixed share of the size rather than a fixed number of
    // pixels, so a notch of the wheel is the same visible change whatever the
    // clock is: five pixels is a fifth of a small clock and nothing at all on
    // a large one.
    //
    // The plain wheel takes the big step and Shift the small one, which is the
    // way round a wheel is actually used: you spin it to get somewhere and
    // then want to creep the last bit, and creeping is the part worth holding
    // a key for.
    const int value = m_size->value();
    const int step = fine ? std::max(1, static_cast<int>(std::lround(value * 0.02)))
                          : std::max(1, static_cast<int>(std::lround(value * 0.10)));
    m_size->setValue(std::clamp(value + steps * step, m_size->minimum(), m_size->maximum()));
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
    out.faceRegen = m_regen->isChecked();
    out.faceRegenMinutes = m_regenMinutes->value();
    out.wireColor = hexOf(m_wire->color());
    out.faceColorRandom = m_faceRandom->isChecked();
    out.wireColorRandom = m_wireRandom->isChecked();
    out.hourMarkColor = hexOf(m_hourMark->color());
    out.minuteMarkColor = hexOf(m_minuteMark->color());
    return out;
}
