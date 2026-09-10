#include "manageclocksdialog.h"

#include "autostart.h"
#include "clockmanager.h"
#include "clockwindow.h"
#include "config.h"
#include "icons.h"
#include "registry.h"

#include <algorithm>

#include <QAbstractItemDelegate>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QApplication>
#include <QFrame>
#include <QMouseEvent>
#include <QPalette>
#include <QCursor>
#include <QDir>
#include <QEvent>
#include <QFileInfo>
#include <QGuiApplication>
#include <QFile>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QScreen>
#include <QShortcut>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

namespace {

// The columns, so the numbers below read as something.
enum Column {
    ColGrip = 0,
    ColShow = 1,
    ColName = 2,
    ColSettings = 3,
    ColTop = 4,
    ColRename = 5,
    ColDelete = 6,
    ColumnCount = 7
};

// The per-row commands sit in columns of their own so each can carry a title.
constexpr int kFirstCommandColumn = ColSettings;

// The file a row stands for, empty while a new row is still being named.
constexpr int kFileRole = Qt::UserRole + 1;

// The tick itself comes from the application's style, which draws checkbox
// indicators with the Font Awesome tick and box -- see installGlyphStyle().
QCheckBox *checkButton(bool checked, const QString &tip)
{
    auto *box = new QCheckBox;
    box->setChecked(checked);
    box->setToolTip(tip);
    return box;
}

// A checkbox centred in its cell rather than jammed against the left edge.
QWidget *centred(QWidget *inner)
{
    auto *holder = new QWidget;
    auto *layout = new QHBoxLayout(holder);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addStretch(1);
    layout->addWidget(inner);
    layout->addStretch(1);
    return holder;
}

QToolButton *iconButton(Glyph glyph, GlyphRole role, const QString &tip)
{
    auto *button = new QToolButton;
    button->setIcon(glyphIcon(glyph, role));
    button->setIconSize(QSize(18, 18));
    button->setAutoRaise(true);
    button->setToolTip(tip);
    button->setFocusPolicy(Qt::NoFocus);
    return button;
}

// The control a row keeps in one of its command columns.  Each sits in a
// holder that centres it under its title, so the cell widget is the holder
// rather than the control itself.
template <typename T>
T *controlIn(const QTableWidget *table, int row, int column)
{
    QWidget *cell = table->cellWidget(row, column);
    return cell ? cell->findChild<T *>() : nullptr;
}

}  // namespace

ManageClocksDialog *ManageClocksDialog::s_instance = nullptr;

void ManageClocksDialog::showDialog(QWidget *parent)
{
    if (!s_instance)
        s_instance = new ManageClocksDialog(parent);
    s_instance->show();
    s_instance->raise();
    s_instance->activateWindow();
}

// Deliberately parentless.  The dialog outlives the clock its menu was opened
// from -- unchecking that clock's Show box closes it, and a child of a window
// being deleted is deleted with it, which would take this dialog down mid-use.
ManageClocksDialog::ManageClocksDialog(QWidget *parent) : QDialog(nullptr)
{
    setWindowTitle(QStringLiteral("Manage clocks"));
    setAttribute(Qt::WA_DeleteOnClose, true);
    // Closing the last clock from here must not take this dialog, and the
    // program, down with it -- there would be no way to make a clock again.
    ClockManager::instance().acquireHold();

    auto *layout = new QVBoxLayout(this);

    auto *blurb = new QLabel(
        QStringLiteral("Show puts a clock on screen. Whatever is showing when vclock "
                       "stops is what comes back when it starts again. Each clock keeps "
                       "its own settings."));
    blurb->setWordWrap(true);
    layout->addWidget(blurb);

    m_table = new QTableWidget(0, ColumnCount, this);
    // The grip has no title: there is no word for it that is not longer than
    // the column, and the icon says what it is.
    m_table->setHorizontalHeaderLabels({QString(), QStringLiteral("Show"),
                                        QStringLiteral("Name"), QStringLiteral("Set"),
                                        QStringLiteral("Top"), QStringLiteral("Name"),
                                        QStringLiteral("Del")});
    m_table->verticalHeader()->setVisible(false);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    // Editing goes through beginEdit() -- the pencil, or a double click on the
    // name -- rather than the view's own triggers, so that the row's buttons
    // always change over with it and a single click or a keystroke can never
    // start a rename by accident.
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->installEventFilter(this);
    m_table->horizontalHeader()->setSectionResizeMode(ColGrip, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(ColShow, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(ColName, QHeaderView::Stretch);
    for (int col = kFirstCommandColumn; col < ColumnCount; ++col)
        m_table->horizontalHeader()->setSectionResizeMode(col, QHeaderView::ResizeToContents);
    layout->addWidget(m_table, 1);

    connect(m_table, &QTableWidget::itemDoubleClicked, this, [this](QTableWidgetItem *item) {
        if (item && item->column() == ColName && !editing())
            beginEdit(item->row());
    });

    // F2 renames, as it does in a file manager.  A shortcut rather than a key
    // handler because the table would otherwise see the key first, and it has
    // its own idea of what F2 means.
    auto *rename = new QShortcut(QKeySequence(Qt::Key_F2), this);
    rename->setContext(Qt::WidgetWithChildrenShortcut);
    connect(rename, &QShortcut::activated, this, [this] {
        const int row = m_table->currentRow();
        if (row >= 0 && !editing())
            beginEdit(row);
    });

    // Delete on the selected row hides that clock: it unticks Show, which is
    // exactly what the box beside it does.  Not what Delete usually means in a
    // list, and deliberately so.  Deleting here erases a config file for good,
    // and a key that does that on one press with nothing selected but a
    // highlight is a key that will one day be pressed by mistake.  Hiding is
    // the reversible neighbour of it, and it is what Esc on the clock itself
    // already does, so the key lands on the thing you can take back.  The Del
    // button in the row still removes the clock, and still asks first.
    auto *hide = new QShortcut(QKeySequence(Qt::Key_Delete), this);
    hide->setContext(Qt::WidgetWithChildrenShortcut);
    connect(hide, &QShortcut::activated, this, [this] {
        const int row = m_table->currentRow();
        if (row < 0 || editing())
            return;
        if (auto *show = controlIn<QCheckBox>(m_table, row, ColShow)) {
            if (show->isChecked())
                show->setChecked(false);  // toggled() closes the clock
        }
    });

    // The delegate tells commit from cancel: Enter reaches commitData first,
    // Escape closes the editor without it.  A click is the awkward case.  Qt
    // looks up the parent chain from whatever was clicked for something that
    // accepts click focus and finds the table, so the editor loses focus --
    // which the delegate calls a commit -- before the button under the pointer
    // is even told it was pressed.  The row's Cancel button therefore cannot
    // cancel by being clicked: by then the edit is over and saved.  So catch
    // it here: a commit arriving with the left button held down over Cancel is
    // that button being pressed, and it means the opposite of a save.
    connect(m_table->itemDelegate(), &QAbstractItemDelegate::commitData, this,
            [this] {
                m_cancelClickPending = cancelPressed();
                m_editCommitted = !m_cancelClickPending;
            });
    connect(m_table->itemDelegate(), &QAbstractItemDelegate::closeEditor, this,
            [this] { finishEdit(m_editCommitted); });

    m_newButton = new QPushButton(glyphIcon(Glyph::New, GlyphRole::Go), QStringLiteral("New clock"));
    m_newButton->setIconSize(QSize(18, 18));
    connect(m_newButton, &QPushButton::clicked, this, &ManageClocksDialog::newClock);

    auto *buttons = new QDialogButtonBox(this);
    buttons->addButton(m_newButton, QDialogButtonBox::ActionRole);
    auto *close = buttons->addButton(QDialogButtonBox::Close);
    close->setIcon(glyphIcon(Glyph::Cancel, GlyphRole::Neutral));
    close->setIconSize(QSize(18, 18));
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::close);

    // Whether the desktop starts vclock at login.  It belongs here rather than
    // in a clock's own settings because it is about the program, not a clock:
    // there is one answer however many clocks are in the list, and what comes
    // back is whatever was showing when the session ended.
    //
    // Left out entirely where the platform has no startup mechanism vclock can
    // write, rather than shown disabled: a box that cannot be ticked invites
    // the question of how to make it tickable, and there is no answer.
    auto *bottom = new QHBoxLayout;
    if (autostart::supported()) {
        m_autostart = new QCheckBox(QStringLiteral("Start at login"));
        m_autostart->setChecked(autostart::enabled());
        m_autostart->setToolTip(
            QStringLiteral("Start vclock when you log in, showing whatever clocks are showing now"));
        connect(m_autostart, &QCheckBox::toggled, this, &ManageClocksDialog::setAutostart);
        bottom->addWidget(m_autostart);
    }

    // How long a clock waits before showing the date under the pointer. Like
    // starting at login, it is one answer for the program rather than a
    // property of any one clock, which is what puts it here.
    m_hoverDelay = new QDoubleSpinBox;
    m_hoverDelay->setDecimals(1);
    m_hoverDelay->setSingleStep(0.5);
    m_hoverDelay->setRange(0.0, kHoverDelayMaxMs / 1000.0);
    m_hoverDelay->setSuffix(QStringLiteral(" s"));
    // No time at all is not a wait anybody would ask for, so the bottom of the
    // range is free to mean the other thing you might want from this setting.
    m_hoverDelay->setSpecialValueText(QStringLiteral("never"));
    m_hoverDelay->setValue(ClockManager::instance().registry().hoverDelayMs / 1000.0);
    m_hoverDelay->setToolTip(
        QStringLiteral("How long to rest the pointer on a clock before it shows the date.\n"
                       "Wind it down past zero to stop it showing at all."));
    auto *hoverLabel = new QLabel(QStringLiteral("Date on hover after"));
    hoverLabel->setBuddy(m_hoverDelay);
    connect(m_hoverDelay, &QDoubleSpinBox::valueChanged, this,
            &ManageClocksDialog::setHoverDelay);

    bottom->addSpacing(12);
    bottom->addWidget(hoverLabel);
    bottom->addWidget(m_hoverDelay);
    bottom->addWidget(buttons, 1);
    layout->addLayout(bottom);

    connect(&ClockManager::instance(), &ClockManager::changed, this, [this] {
        // A clock closed from its own menu changes the Show column, but not
        // while the user is halfway through typing a name here.
        if (!editing())
            scheduleRebuild();
    });

    rebuild();
    resize(520, 320);

    // No parent means no automatic placement, so it is put over the clock it
    // was opened from rather than wherever the window manager fancies.
    if (parent) {
        if (QScreen *screen = parent->screen()) {
            const QRect area = screen->availableGeometry();
            QRect where(QPoint(0, 0), size());
            where.moveCenter(parent->frameGeometry().center());
            if (!area.contains(where))
                where.moveCenter(area.center());
            move(where.topLeft());
        }
    }
}

ManageClocksDialog::~ManageClocksDialog()
{
    if (s_instance == this)
        s_instance = nullptr;
    // Releasing may end the program, if this dialog was the only thing keeping
    // it alive; that is the intent.
    ClockManager::instance().releaseHold();
}

void ManageClocksDialog::rebuild()
{
    m_populating = true;
    m_table->setRowCount(0);
    const ClockManager &manager = ClockManager::instance();
    for (const ClockEntry &entry : manager.registry().clocks)
        addRow(entry.file, entry.name, manager.isOpen(entry.path()));
    m_populating = false;
}

void ManageClocksDialog::addRow(const QString &file, const QString &name, bool open)
{
    const int row = m_table->rowCount();
    m_table->insertRow(row);

    auto *grip = iconButton(Glyph::Grip, GlyphRole::Neutral,
                            QStringLiteral("Drag to move this clock up or down the list"));
    grip->setObjectName(QStringLiteral("grip"));
    // Not a button in the sense of being pressed and released to do something,
    // so it does not take focus and its clicks are read by the dialog's event
    // filter rather than by a handler of its own.
    grip->setFocusPolicy(Qt::NoFocus);
    if (isDefaultClockFile(file)) {
        grip->setEnabled(false);
        grip->setToolTip(QStringLiteral("The default clock stays at the top of the list"));
    } else {
        grip->setCursor(Qt::OpenHandCursor);
        grip->installEventFilter(this);
    }
    m_table->setCellWidget(row, ColGrip, centred(grip));

    auto *show = checkButton(open, QStringLiteral("Put this clock on screen"));
    show->setObjectName(QStringLiteral("show"));
    connect(show, &QCheckBox::toggled, this, [this, show](bool on) {
        if (m_populating)
            return;
        const int r = rowOfWidget(show);
        if (r >= 0)
            toggleOpen(r, on);
    });
    m_table->setCellWidget(row, ColShow, centred(show));


    auto *item = new QTableWidgetItem(name);
    item->setData(kFileRole, file);
    item->setFlags(item->flags() & ~Qt::ItemIsEditable);
    m_table->setItem(row, ColName, item);

    auto *settings = iconButton(Glyph::Settings, GlyphRole::Neutral,
                                QStringLiteral("Settings for this clock"));
    settings->setObjectName(QStringLiteral("settings"));
    connect(settings, &QToolButton::clicked, this, [this, settings] {
        const int r = rowOfWidget(settings);
        if (r >= 0 && !editing())
            openRowSettings(r);
    });
    m_table->setCellWidget(row, ColSettings, centred(settings));

    auto *onTop = checkButton(alwaysOnTopOf(file),
                              QStringLiteral("Keep this clock above other windows"));
    onTop->setObjectName(QStringLiteral("ontop"));
    connect(onTop, &QCheckBox::toggled, this, [this, onTop](bool on) {
        if (m_populating)
            return;
        const int r = rowOfWidget(onTop);
        if (r >= 0)
            setAlwaysOnTop(r, on);
    });
    m_table->setCellWidget(row, ColTop, centred(onTop));

    // The default clock cannot be renamed, its file being the one the program
    // falls back to, so rather than leaving a dead button in its row the space
    // does the useful thing you would want there instead: make a clock of your
    // own that starts out as a copy of it.
    if (isDefaultClockFile(file)) {
        auto *clone = iconButton(Glyph::Clone, GlyphRole::Go,
                                 QStringLiteral("New clock, copying this one"));
        clone->setObjectName(QStringLiteral("clone"));
        connect(clone, &QToolButton::clicked, this, [this, clone] {
            const int r = rowOfWidget(clone);
            if (r >= 0 && !editing())
                cloneClock(fileAt(r));
        });
        m_table->setCellWidget(row, ColRename, centred(clone));
    } else {
        auto *edit = iconButton(Glyph::Edit, GlyphRole::Neutral, QStringLiteral("Rename"));
        edit->setObjectName(QStringLiteral("edit"));
        connect(edit, &QToolButton::clicked, this, [this, edit] {
            const int r = rowOfWidget(edit);
            if (r < 0)
                return;
            if (m_editRow == r)
                m_table->closePersistentEditor(m_table->item(r, ColName));
            else if (!editing())
                beginEdit(r);
        });
        m_table->setCellWidget(row, ColRename, centred(edit));
    }

    auto *remove = iconButton(Glyph::Delete, GlyphRole::Stop, QStringLiteral("Delete"));
    remove->setObjectName(QStringLiteral("delete"));
    // The default config is what a clock started with no --config writes, so
    // there is always one of it; it can be hidden, but not renamed or removed.
    if (isDefaultClockFile(file)) {
        remove->setEnabled(false);
        remove->setToolTip(QStringLiteral("The default clock cannot be deleted"));
    }
    connect(remove, &QToolButton::clicked, this, [this, remove] { removeClicked(remove); });
    m_table->setCellWidget(row, ColDelete, centred(remove));
}

int ManageClocksDialog::rowOfWidget(QWidget *widget) const
{
    for (int row = 0; row < m_table->rowCount(); ++row) {
        for (int col = 0; col < ColumnCount; ++col) {
            const QWidget *cell = m_table->cellWidget(row, col);
            if (cell && (cell == widget || cell->isAncestorOf(widget)))
                return row;
        }
    }
    return -1;
}

QString ManageClocksDialog::fileAt(int row) const
{
    const QTableWidgetItem *item = m_table->item(row, ColName);
    return item ? item->data(kFileRole).toString() : QString();
}

// ---------------------------------------------------------------- editing

void ManageClocksDialog::beginEdit(int row)
{
    QTableWidgetItem *item = m_table->item(row, ColName);
    if (!item)
        return;
    // The default clock's file is the one the program falls back to, so its
    // name is not the user's to change.  Refused here rather than only on the
    // pencil, since a double click and F2 arrive by another route.
    if (isDefaultClockFile(fileAt(row)))
        return;
    m_cancelClickPending = false;
    m_cloneFrom.clear();
    m_editRow = row;
    m_editWasNamed = item->text();
    m_editIsNew = fileAt(row).isEmpty();
    m_editCommitted = false;

    setRowEditing(row, true);
    item->setFlags(item->flags() | Qt::ItemIsEditable);
    m_table->setCurrentItem(item);
    m_table->editItem(item);
}

// Swap the row's pencil and bin for a save and a cancel while it is being
// edited, and keep the rest of the dialog out of the way until it is done.
void ManageClocksDialog::setRowEditing(int row, bool on)
{
    m_newButton->setEnabled(!on);

    if (auto *gear = controlIn<QToolButton>(m_table, row, ColSettings))
        gear->setEnabled(!on);
    // Renaming is about the row's name, not the clock on screen; raising it
    // mid-edit would be a second, unrelated change, so the box waits.
    if (auto *top = controlIn<QCheckBox>(m_table, row, ColTop))
        top->setEnabled(!on);
    auto *edit = controlIn<QToolButton>(m_table, row, ColRename);
    auto *remove = controlIn<QToolButton>(m_table, row, ColDelete);
    if (edit) {
        edit->setIcon(glyphIcon(on ? Glyph::Save : Glyph::Edit,
                                on ? GlyphRole::Go : GlyphRole::Neutral));
        edit->setToolTip(on ? QStringLiteral("Save (Enter)") : QStringLiteral("Rename"));
    }
    if (remove) {
        remove->setIcon(glyphIcon(on ? Glyph::Cancel : Glyph::Delete, GlyphRole::Stop));
        remove->setToolTip(on ? QStringLiteral("Cancel (Esc)") : QStringLiteral("Delete"));
        remove->setEnabled(on || fileAt(row) != QLatin1String("default.cfg"));
    }
    // While editing, the bin is the cancel button; the delete path must not
    // fire from it, so it is rewired for the duration.  The click itself is
    // handled where the editor is committed, because that happens first --
    // see the commitData connection -- but the button is still wired up, for
    // the keyboard and for the case where the editor has already gone.
    if (remove) {
        disconnect(remove, nullptr, this, nullptr);
        if (on) {
            connect(remove, &QToolButton::clicked, this, [this] {
                m_cancelClickPending = false;
                if (editing())
                    finishEdit(false);
            });
        } else {
            connect(remove, &QToolButton::clicked, this,
                    [this, remove] { removeClicked(remove); });
        }
    }
}

// ---------------------------------------------------------------- reordering

bool ManageClocksDialog::eventFilter(QObject *watched, QEvent *event)
{
    // Space on the highlighted row shows or hides that clock.  It is the one
    // thing in a row worth reaching for without the mouse, and space is where
    // a list puts its tick: the Show box is the row's own checkbox, so the key
    // that ticks a checkbox ticks it.  Read here rather than as a shortcut so
    // that it only means this while the list itself has the focus -- the boxes
    // and buttons elsewhere in the dialog keep their own space.
    if (watched == m_table && event->type() == QEvent::KeyPress) {
        auto *key = static_cast<QKeyEvent *>(event);
        if (key->key() == Qt::Key_Space && !key->modifiers() && !editing()) {
            const int row = m_table->currentRow();
            if (row >= 0) {
                if (auto *show = controlIn<QCheckBox>(m_table, row, ColShow))
                    show->setChecked(!show->isChecked());  // toggled() opens or closes it
            }
            return true;
        }
    }

    auto *grip = qobject_cast<QToolButton *>(watched);
    if (!grip)
        return QDialog::eventFilter(watched, event);

    switch (event->type()) {
    case QEvent::MouseButtonPress: {
        auto *mouse = static_cast<QMouseEvent *>(event);
        // Pressing anywhere moves the focus before the press is delivered, so
        // by now a name being edited has already been saved by this very
        // press.  That press was the user finishing the edit, not reaching for
        // the grip, so it does not also start a drag.
        if (mouse->button() == Qt::LeftButton && !editing() && !m_editJustEnded)
            startDrag(rowOfWidget(grip), mouse->globalPosition().toPoint());
        return true;  // the grip is dragged, never clicked
    }
    case QEvent::MouseMove:
        if (m_dragRow >= 0)
            updateDrag(static_cast<QMouseEvent *>(event)->globalPosition().toPoint());
        return true;
    case QEvent::MouseButtonRelease:
        if (m_dragRow >= 0)
            endDrag(true);
        return true;
    // A drag interrupted -- the window deactivated, Escape pressed -- puts the
    // list back rather than dropping the row wherever the pointer stopped.
    case QEvent::FocusOut:
    case QEvent::WindowDeactivate:
        if (m_dragRow >= 0)
            endDrag(false);
        break;
    default:
        break;
    }
    return QDialog::eventFilter(watched, event);
}

int ManageClocksDialog::firstMovableRow() const
{
    for (int row = 0; row < m_table->rowCount(); ++row)
        if (!isDefaultClockFile(fileAt(row)))
            return row;
    return m_table->rowCount();
}

void ManageClocksDialog::startDrag(int row, const QPoint &globalPos)
{
    if (row < 0 || isDefaultClockFile(fileAt(row)))
        return;
    m_dragRow = row;
    m_dragging = false;
    m_dragFrom = globalPos;
    m_dropIndex = row;
}

int ManageClocksDialog::dropIndexAt(int viewportY) const
{
    // The boundary the row would go to is the one nearest the pointer, so the
    // line follows the pointer over the gap it is closest to rather than
    // waiting until a whole row has been passed.
    int index = m_table->rowCount();
    for (int row = 0; row < m_table->rowCount(); ++row) {
        const int top = m_table->rowViewportPosition(row);
        if (viewportY < top + m_table->rowHeight(row) / 2) {
            index = row;
            break;
        }
    }
    return std::max(index, firstMovableRow());
}

void ManageClocksDialog::updateDrag(const QPoint &globalPos)
{
    if (!m_dragging) {
        if ((globalPos - m_dragFrom).manhattanLength() < QApplication::startDragDistance())
            return;
        m_dragging = true;
        if (QWidget *held = m_table->cellWidget(m_dragRow, ColGrip))
            if (auto *grip = held->findChild<QToolButton *>())
                grip->setCursor(Qt::ClosedHandCursor);
    }

    const QPoint local = m_table->viewport()->mapFromGlobal(globalPos);
    m_dropIndex = dropIndexAt(local.y());

    // A drop back where the row already is would change nothing, and a line
    // drawn at either of its own edges says otherwise, so it is left off.
    if (m_dropIndex == m_dragRow || m_dropIndex == m_dragRow + 1) {
        if (m_dropLine)
            m_dropLine->hide();
        return;
    }
    if (!m_dropLine) {
        // Drawn as a filled block rather than a frame: a frame paints itself in
        // the palette's shadow colours, which on this row is no colour at all.
        // A plain filled block.  A frame would paint itself in the palette's
        // shadow colours, which against a table row is barely a line at all,
        // and the table paints over a palette background, so the colour is set
        // as a style rather than as a brush.
        m_dropLine = new QFrame(m_table->viewport());
        m_dropLine->setFrameShape(QFrame::NoFrame);
        m_dropLine->setAttribute(Qt::WA_TransparentForMouseEvents);
        m_dropLine->setStyleSheet(QStringLiteral("background-color: %1;")
                                      .arg(glyphColor(GlyphRole::Info).name()));
    }
    const int y = m_dropIndex < m_table->rowCount()
                      ? m_table->rowViewportPosition(m_dropIndex)
                      : m_table->rowViewportPosition(m_table->rowCount() - 1)
                            + m_table->rowHeight(m_table->rowCount() - 1);
    m_dropLine->setGeometry(0, std::max(0, y - 1), m_table->viewport()->width(), 2);
    m_dropLine->show();
    m_dropLine->raise();
}

void ManageClocksDialog::endDrag(bool dropped)
{
    const int from = m_dragRow;
    const int to = m_dropIndex;
    const bool moved = m_dragging;
    if (QWidget *held = m_table->cellWidget(from, ColGrip))
        if (auto *grip = held->findChild<QToolButton *>())
            grip->setCursor(Qt::OpenHandCursor);
    m_dragRow = -1;
    m_dragging = false;
    m_dropIndex = -1;
    if (m_dropLine)
        m_dropLine->hide();
    if (dropped && moved)
        moveClock(from, to);
}

void ManageClocksDialog::moveClock(int from, int to)
{
    Registry registry = ClockManager::instance().registry();
    if (from < 0 || from >= int(registry.clocks.size()))
        return;
    // An index counted with the row still in place means one thing before the
    // row is taken out and another after it, so it is corrected here rather
    // than everywhere it is worked out.
    if (to > from)
        --to;
    to = std::clamp(to, 0, int(registry.clocks.size()) - 1);
    if (to == from)
        return;
    const ClockEntry entry = registry.clocks.at(from);
    registry.clocks.erase(registry.clocks.begin() + from);
    registry.clocks.insert(registry.clocks.begin() + to, entry);
    ClockManager::instance().setRegistry(registry);
    // The list is rebuilt from the registry, so the row that moved is picked
    // out again afterwards: it is the one the user was just holding.
    QTimer::singleShot(0, this, [this, file = entry.file] {
        for (int row = 0; row < m_table->rowCount(); ++row)
            if (fileAt(row) == file) {
                m_table->setCurrentCell(row, ColName);
                return;
            }
    });
}

void ManageClocksDialog::removeClicked(QToolButton *button)
{
    // The press that opened this click may have been the one that cancelled an
    // edit, in which case the button had already turned back into the bin by
    // the time the mouse came up.  That click belongs to the cancel.
    if (m_cancelClickPending) {
        m_cancelClickPending = false;
        return;
    }
    const int row = rowOfWidget(button);
    if (row >= 0 && !editing())
        deleteRow(row);
}

bool ManageClocksDialog::cancelPressed() const
{
    if (m_editRow < 0 || !(QGuiApplication::mouseButtons() & Qt::LeftButton))
        return false;
    auto *remove = controlIn<QToolButton>(m_table, m_editRow, ColDelete);
    return remove && remove->isVisible()
           && remove->rect().contains(remove->mapFromGlobal(QCursor::pos()));
}

void ManageClocksDialog::finishEdit(bool committed)
{
    if (m_editRow < 0)
        return;
    const int row = m_editRow;
    const bool wasNew = m_editIsNew;
    const QString cloneFrom = m_cloneFrom;
    const QString previous = m_editWasNamed;
    m_cloneFrom.clear();
    // Set for the rest of this event only, so that whatever ended the edit --
    // a click on some other row's grip, say -- is not read as a second action.
    m_editJustEnded = true;
    QTimer::singleShot(0, this, [this] { m_editJustEnded = false; });
    // Cleared first: closing the editor below re-enters through closeEditor.
    m_editRow = -1;
    m_editIsNew = false;
    m_editCommitted = false;

    QTableWidgetItem *item = m_table->item(row, ColName);
    if (!item) {
        rebuild();
        return;
    }
    item->setFlags(item->flags() & ~Qt::ItemIsEditable);

    // A name is a file name now, so a .cfg the user typed is theirs to leave
    // off -- the program puts it on.
    const QString typed = cleanClockName(item->text());
    // A name saved blank is not a name.  On a row that was only just added
    // there is nothing to keep, so it goes; on one that already existed it
    // means the same as having cancelled.
    if (!committed || typed.isEmpty()) {
        if (wasNew) {
            m_table->removeRow(row);
            setRowEditing(row, false);
            m_newButton->setEnabled(true);
            return;
        }
        item->setText(previous);
        setRowEditing(row, false);
        return;
    }

    Registry registry = ClockManager::instance().registry();
    const QString file = fileAt(row);
    const QString problem = clockNameError(typed, registry, file);
    if (!problem.isEmpty()) {
        item->setText(typed);
        m_cloneFrom = cloneFrom;  // the edit is not over after all
        refuseName(row, problem, previous);
        return;
    }

    item->setText(typed);
    setRowEditing(row, false);

    if (wasNew) {
        ClockEntry entry;
        entry.file = clockFileName(typed);
        entry.name = typed;
        entry.show = true;
        const QString path = entry.path();
        // A clone is a copy of the settings, made before the clock is opened so
        // that it comes up already looking the part rather than appearing plain
        // and changing under the user a moment later.  A source that was never
        // written is not an error: there is nothing saved to differ from the
        // defaults the new clock starts with anyway.
        const int sourceIndex = registry.indexOfFile(cloneFrom);
        const QString sourcePath =
            sourceIndex >= 0 ? registry.clocks.at(sourceIndex).path() : QString();
        const bool cloned = !cloneFrom.isEmpty()
                            && copyClockFile(registry, cloneFrom, path);
        if (cloned && sourceIndex >= 0)
            registry.clocks[sourceIndex].show = false;
        registry.clocks.push_back(entry);
        item->setData(kFileRole, entry.file);
        ClockManager::instance().setRegistry(registry);
        const QString source = cloned ? sourcePath : QString();
        QTimer::singleShot(0, this, [path, cloned, source] {
            ClockManager &manager = ClockManager::instance();
            // A clock made from nothing is about to have its settings put in
            // front of the user, and a settings window belongs to its clock:
            // it is given the focus along with it.  A clone is only put on
            // screen, so the list keeps the keyboard.
            if (!manager.isOpen(path))
                manager.openClock(path, cloned ? ClockManager::Focus::Leave
                                               : ClockManager::Focus::Take);
            // A copy carries the original's place on screen along with its
            // looks, so it comes up exactly on top of what it was copied from
            // and there would be no sign anything had happened.  The original
            // goes down instead: what is left on screen is the clock you are
            // now working on, in the place the one before it had.  It is put
            // up first, so the program is never briefly without a clock.
            if (cloned && !source.isEmpty())
                manager.closeClock(source);
            // A clock you have just made from nothing is one you have something
            // in mind for, so its settings come up with it rather than waiting
            // to be asked for.  A clone already looks how you wanted it to, so
            // it is simply put on screen.
            if (!cloned)
                if (ClockWindow *clock = manager.clockAt(path))
                    clock->openSettings();
        });
        return;
    }

    const int index = registry.indexOfFile(file);
    if (index < 0)
        return;

    // The settings follow the name, so that what is in the config directory
    // can still be read off the list of clocks.  A clock started from a path
    // of the user's own is renamed where it stands rather than being dragged
    // into the config directory.
    const QString oldPath = registry.clocks[index].path();
    const QString leaf = clockFileName(typed);
    const bool ownPath = QDir::isAbsolutePath(file);
    const QString newFile = ownPath ? QDir(QFileInfo(oldPath).absolutePath()).filePath(leaf) : leaf;
    if (newFile != file) {
        const QString newPath =
            ownPath ? newFile : QDir(configDir()).filePath(newFile);
        QString error;
        if (!ClockManager::instance().moveClockFile(oldPath, newPath, &error)) {
            refuseName(row, error, previous);
            return;
        }
        registry.clocks[index].file = newFile;
        item->setData(kFileRole, newFile);
    }
    registry.clocks[index].name = typed;
    ClockManager::instance().setRegistry(registry);
}

// Tell the user why the name will not do, and put them back in the editor once
// they have read it.  Modal and with nothing but OK on it, because it is the
// answer to something they just did and there is only one way on from it.
//
// The editor is re-opened on the next turn of the event loop rather than here,
// since this runs inside the view's own closeEditor handling.  What they typed
// is left in it to be corrected, while cancelling still goes back to the name
// the clock actually has -- the refused one was never stored anywhere.
void ManageClocksDialog::refuseName(int row, const QString &reason, const QString &fallback)
{
    QMessageBox box(QMessageBox::Warning, QStringLiteral("Rename clock"), reason,
                    QMessageBox::Ok, this);
    box.exec();
    // What the row was going to be is put back as well as what it was called:
    // a name refused while naming a clone still leaves a clone to be named,
    // not a blank clock.
    const QString cloneFrom = m_cloneFrom;
    QTimer::singleShot(0, this, [this, row, fallback, cloneFrom] {
        if (row >= m_table->rowCount() || editing())
            return;
        beginEdit(row);
        if (m_editRow == row) {
            m_editWasNamed = fallback;
            m_cloneFrom = cloneFrom;
        }
    });
}

// ---------------------------------------------------------------- actions

void ManageClocksDialog::newClock()
{
    if (editing())
        return;
    addRow(QString(), QString(), false);
    beginEdit(m_table->rowCount() - 1);
}

void ManageClocksDialog::cloneClock(const QString &sourceFile)
{
    if (editing() || sourceFile.isEmpty())
        return;
    newClock();
    // Set after the row is being edited, since beginEdit clears it: the copy
    // is made in finishEdit, once the new clock has a file to be copied into.
    if (editing())
        m_cloneFrom = sourceFile;
}

bool ManageClocksDialog::copyClockFile(const Registry &registry,
                                       const QString &sourceFile,
                                       const QString &toPath)
{
    const int index = registry.indexOfFile(sourceFile);
    if (index < 0)
        return false;
    const QString from = registry.clocks.at(index).path();
    // Whatever the source clock is holding is written out first, or the copy
    // would be of the file as it stood before the last change to the clock on
    // screen -- which is the one the user is looking at as they clone it.
    ClockManager &manager = ClockManager::instance();
    if (ClockWindow *clock = manager.clockAt(from))
        clock->flushSave();
    if (!QFileInfo::exists(from))
        return false;
    return QFile::copy(from, toPath);
}

void ManageClocksDialog::deleteRow(int row)
{
    const QString file = fileAt(row);
    if (file.isEmpty() || file == QLatin1String("default.cfg"))
        return;
    Registry registry = ClockManager::instance().registry();
    const int index = registry.indexOfFile(file);
    if (index < 0)
        return;
    const ClockEntry entry = registry.clocks.at(index);

    const QMessageBox::StandardButton answer = QMessageBox::question(
        this, QStringLiteral("Delete clock"),
        QStringLiteral("Delete \u201c%1\u201d and the settings it has saved?\n\n"
                       "This cannot be undone.")
            .arg(entry.name.isEmpty() ? entry.file : entry.name),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer != QMessageBox::Yes)
        return;

    // Taken off screen before its config goes, so that closing it cannot write
    // the file back out again.
    ClockManager::instance().closeClock(entry.path());
    QFile::remove(entry.path());
    registry.clocks.removeAt(index);
    ClockManager::instance().setRegistry(registry);
    rebuild();
}

void ManageClocksDialog::toggleOpen(int row, bool open)
{
    Registry registry = ClockManager::instance().registry();
    const int index = registry.indexOfFile(fileAt(row));
    if (index < 0)
        return;
    const QString path = registry.clocks.at(index).path();
    // Shown without taking the keyboard: the user is still in this list, quite
    // possibly about to tick the next row, and a clock that grabs the focus as
    // it appears makes the following keystroke go somewhere they were not
    // looking.  It still comes to the front, so the tick visibly does
    // something.
    if (open)
        ClockManager::instance().openClock(path, ClockManager::Focus::Leave);
    else
        ClockManager::instance().closeClock(path);
}

bool ManageClocksDialog::alwaysOnTopOf(const QString &file) const
{
    const Config defaults;
    if (file.isEmpty())
        return defaults.alwaysOnTop;
    const Registry &registry = ClockManager::instance().registry();
    const int index = registry.indexOfFile(file);
    if (index < 0)
        return defaults.alwaysOnTop;
    const QString path = registry.clocks.at(index).path();
    // A clock on screen may have been changed since it was last written out,
    // so ask the window first and fall back to the file for one that is down.
    if (const ClockWindow *clock = ClockManager::instance().clockAt(path))
        return clock->cfg().alwaysOnTop;
    return loadConfig(path).alwaysOnTop;
}

void ManageClocksDialog::setAlwaysOnTop(int row, bool on)
{
    const QString file = fileAt(row);
    if (file.isEmpty())
        return;
    const Registry &registry = ClockManager::instance().registry();
    const int index = registry.indexOfFile(file);
    if (index < 0)
        return;
    const QString path = registry.clocks.at(index).path();
    // A clock that is showing has to be raised or dropped there and then; one
    // that is not has only its config, which is what it reads when it opens.
    if (ClockWindow *clock = ClockManager::instance().clockAt(path)) {
        clock->setAlwaysOnTop(on);
        return;
    }
    Config cfg = loadConfig(path);
    if (cfg.alwaysOnTop == on)
        return;
    cfg.alwaysOnTop = on;
    saveConfig(cfg, path);
}

// The setting can also be changed from a clock's own menu or its settings,
// neither of which comes back through here, so the boxes are read again
// whenever the dialog is brought to the front.
void ManageClocksDialog::refreshAlwaysOnTop()
{
    for (int row = 0; row < m_table->rowCount(); ++row) {
        auto *box = controlIn<QCheckBox>(m_table, row, ColTop);
        if (!box)
            continue;
        const bool on = alwaysOnTopOf(fileAt(row));
        if (box->isChecked() != on) {
            const QSignalBlocker blocker(box);
            box->setChecked(on);
        }
    }
}

void ManageClocksDialog::changeEvent(QEvent *event)
{
    QDialog::changeEvent(event);
    if (event->type() == QEvent::ActivationChange && isActiveWindow())
        refreshAlwaysOnTop();
}

void ManageClocksDialog::scheduleRebuild()
{
    if (m_rebuildQueued)
        return;
    m_rebuildQueued = true;
    QTimer::singleShot(0, this, [this] {
        m_rebuildQueued = false;
        // Rebuilding destroys the row widgets, and one of them is the grip the
        // pointer is holding, so a list that changes under a drag waits until
        // the drag is over.
        if (m_dragRow >= 0) {
            scheduleRebuild();
            return;
        }
        if (!editing())
            rebuild();
    });
}

// The gear needs the clock itself, not just its config, so a hidden one is put
// on screen first; the Show box follows because opening it tells the manager,
// which tells us.
void ManageClocksDialog::openRowSettings(int row)
{
    const QString file = fileAt(row);
    if (file.isEmpty())
        return;
    const Registry &registry = ClockManager::instance().registry();
    const int index = registry.indexOfFile(file);
    if (index < 0)
        return;
    const QString path = registry.clocks.at(index).path();

    // Deferred: opening the clock rebuilds the table, which deletes the button
    // whose click brought us here.
    QTimer::singleShot(0, this, [path] {
        ClockManager &manager = ClockManager::instance();
        if (!manager.isOpen(path))
            manager.openClock(path);
        if (ClockWindow *clock = manager.clockAt(path))
            clock->openSettings();
    });
}

void ManageClocksDialog::commitRegistry()
{
    ClockManager::instance().setRegistry(ClockManager::instance().registry());
}

// Writing the entry can fail -- a read-only home, a full disk -- and a box
// that stays ticked when nothing was written would be a lie, so the box goes
// back to what is actually on disk and says why.
void ManageClocksDialog::setAutostart(bool on)
{
    if (autostart::setEnabled(on))
        return;

    QMessageBox::warning(this, QStringLiteral("Start at login"),
                         QStringLiteral("Could not %1 the startup entry: %2.")
                             .arg(on ? QStringLiteral("write") : QStringLiteral("remove"),
                                  autostart::reason()));
    const QSignalBlocker block(m_autostart);
    m_autostart->setChecked(autostart::enabled());
}

// The clocks read this as they are hovered rather than being told about it, so
// storing it is the whole job.
void ManageClocksDialog::setHoverDelay(double seconds)
{
    Registry registry = ClockManager::instance().registry();
    const int ms = qRound(seconds * 1000.0);
    if (ms == registry.hoverDelayMs)
        return;
    registry.hoverDelayMs = ms;
    ClockManager::instance().setRegistry(registry);
}
