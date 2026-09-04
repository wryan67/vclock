#include "manageclocksdialog.h"

#include "autostart.h"
#include "clockmanager.h"
#include "clockwindow.h"
#include "config.h"
#include "icons.h"
#include "registry.h"

#include <QAbstractItemDelegate>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFile>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QScreen>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

namespace {

// The columns, so the numbers below read as something.
enum Column { ColShow = 0, ColName = 1, ColActions = 2, ColumnCount = 3 };

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
    m_table->setHorizontalHeaderLabels(
        {QStringLiteral("Show"), QStringLiteral("Name"), QString()});
    m_table->verticalHeader()->setVisible(false);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    // Editing goes through beginEdit() -- the pencil, or a double click on the
    // name -- rather than the view's own triggers, so that the row's buttons
    // always change over with it and a single click or a keystroke can never
    // start a rename by accident.
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->horizontalHeader()->setSectionResizeMode(ColShow, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(ColName, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(ColActions, QHeaderView::ResizeToContents);
    layout->addWidget(m_table, 1);

    connect(m_table, &QTableWidget::itemDoubleClicked, this, [this](QTableWidgetItem *item) {
        if (item && item->column() == ColName && !editing())
            beginEdit(item->row());
    });

    // The delegate tells commit from cancel: Enter reaches commitData first,
    // Escape closes the editor without it.
    connect(m_table->itemDelegate(), &QAbstractItemDelegate::commitData, this,
            [this] { m_editCommitted = true; });
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

    auto *actions = new QWidget;
    auto *actionLayout = new QHBoxLayout(actions);
    actionLayout->setContentsMargins(2, 0, 2, 0);
    actionLayout->setSpacing(2);

    auto *settings = iconButton(Glyph::Settings, GlyphRole::Neutral,
                                QStringLiteral("Settings for this clock"));
    settings->setObjectName(QStringLiteral("settings"));
    connect(settings, &QToolButton::clicked, this, [this, settings] {
        const int r = rowOfWidget(settings);
        if (r >= 0 && !editing())
            openRowSettings(r);
    });
    actionLayout->addWidget(settings);

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
    actionLayout->addWidget(edit);

    auto *remove = iconButton(Glyph::Delete, GlyphRole::Stop, QStringLiteral("Delete"));
    remove->setObjectName(QStringLiteral("delete"));
    // The default config is what a clock started with no --config writes, so
    // there is always one of it; it can be renamed and hidden, but not removed.
    if (file == QLatin1String("default.cfg")) {
        remove->setEnabled(false);
        remove->setToolTip(QStringLiteral("The default clock cannot be deleted"));
    }
    connect(remove, &QToolButton::clicked, this, [this, remove] {
        const int r = rowOfWidget(remove);
        if (r >= 0 && !editing())
            deleteRow(r);
    });
    actionLayout->addWidget(remove);

    m_table->setCellWidget(row, ColActions, actions);
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

    QWidget *actions = m_table->cellWidget(row, ColActions);
    if (!actions)
        return;
    if (auto *gear = actions->findChild<QToolButton *>(QStringLiteral("settings")))
        gear->setEnabled(!on);
    auto *edit = actions->findChild<QToolButton *>(QStringLiteral("edit"));
    auto *remove = actions->findChild<QToolButton *>(QStringLiteral("delete"));
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
    // fire from it, so it is rewired for the duration.
    if (remove) {
        disconnect(remove, nullptr, this, nullptr);
        if (on) {
            connect(remove, &QToolButton::clicked, this, [this] {
                if (QTableWidgetItem *item = m_table->item(m_editRow, ColName))
                    m_table->closePersistentEditor(item);
                finishEdit(false);
            });
        } else {
            connect(remove, &QToolButton::clicked, this, [this, remove] {
                const int r = rowOfWidget(remove);
                if (r >= 0 && !editing())
                    deleteRow(r);
            });
        }
    }
}

void ManageClocksDialog::finishEdit(bool committed)
{
    if (m_editRow < 0)
        return;
    const int row = m_editRow;
    const bool wasNew = m_editIsNew;
    const QString previous = m_editWasNamed;
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

    const QString typed = item->text().trimmed();
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

    item->setText(typed);
    setRowEditing(row, false);

    Registry registry = ClockManager::instance().registry();
    if (wasNew) {
        ClockEntry entry;
        entry.file = registry.uniqueFileFor(typed);
        entry.name = typed;
        entry.show = true;
        registry.clocks.push_back(entry);
        item->setData(kFileRole, entry.file);
        ClockManager::instance().setRegistry(registry);
        // A clock you have just made and named is one you want to see.
        ClockManager::instance().openClock(entry.path());
        return;
    }

    const int index = registry.indexOfFile(fileAt(row));
    if (index >= 0) {
        registry.clocks[index].name = typed;
        ClockManager::instance().setRegistry(registry);
    }
}

// ---------------------------------------------------------------- actions

void ManageClocksDialog::newClock()
{
    if (editing())
        return;
    addRow(QString(), QString(), false);
    beginEdit(m_table->rowCount() - 1);
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
    if (open)
        ClockManager::instance().openClock(path);
    else
        ClockManager::instance().closeClock(path);
}


void ManageClocksDialog::scheduleRebuild()
{
    if (m_rebuildQueued)
        return;
    m_rebuildQueued = true;
    QTimer::singleShot(0, this, [this] {
        m_rebuildQueued = false;
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
