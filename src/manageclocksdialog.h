// The list of clocks, with a name you can edit in place.
#pragma once

#include <QDialog>

class QCheckBox;
class QDoubleSpinBox;
class QEvent;
class QPushButton;
class QTableWidget;
class QToolButton;
class QTableWidgetItem;
class QWidget;

class ManageClocksDialog : public QDialog
{
    Q_OBJECT

public:
    // One dialog for the whole program, however many clocks are up: it edits a
    // single shared list, so a second copy could only disagree with the first.
    static void showDialog(QWidget *parent);

    ~ManageClocksDialog() override;

private:
    explicit ManageClocksDialog(QWidget *parent);

    void rebuild();
    void addRow(const QString &file, const QString &name, bool open);
    void newClock();

    void beginEdit(int row);
    void finishEdit(bool committed);
    // Refuse a name, say why, and go back into the editor.
    void refuseName(int row, const QString &reason, const QString &fallback);
    bool editing() const { return m_editRow >= 0; }
    // Whether the row's Cancel button is being pressed right now.
    bool cancelPressed() const;
    void setRowEditing(int row, bool on);

    void deleteRow(int row);
    // The bin was clicked: the shared body of the handler, wired up both when
    // a row is built and when it stops being edited.
    void removeClicked(QToolButton *button);
    // Open the settings for a row's clock, putting the clock on screen first if
    // it is hidden -- there is nothing to change the look of otherwise.
    void openRowSettings(int row);
    void toggleOpen(int row, bool open);

    // Whether a clock is set to stay above other windows.  A clock that is not
    // on screen has no window to ask, so its config file is read instead.
    bool alwaysOnTopOf(const QString &file) const;
    void setAlwaysOnTop(int row, bool on);
    void refreshAlwaysOnTop();

    void changeEvent(QEvent *event) override;

    // Turn starting at login on or off, and put the box back if it fails.
    void setAutostart(bool on);

    // Store the new wait before a clock shows the date under the pointer.
    void setHoverDelay(double seconds);

    // Which row a per-row button belongs to.  Rows shift as clocks are added
    // and removed, so the button is found rather than its index remembered.
    int rowOfWidget(QWidget *widget) const;
    QString fileAt(int row) const;

    void commitRegistry();
    // Rebuild on the next pass of the event loop.  A rebuild throws away the
    // row widgets, and it is usually a row widget's own signal that asked for
    // it, so it cannot be done there and then.
    void scheduleRebuild();

    static ManageClocksDialog *s_instance;

    QTableWidget *m_table = nullptr;
    QPushButton *m_newButton = nullptr;
    QCheckBox *m_autostart = nullptr;
    QDoubleSpinBox *m_hoverDelay = nullptr;

    // -1 when nothing is being edited.
    int m_editRow = -1;
    QString m_editWasNamed;   // the name to go back to on cancel
    // A press on Cancel ends the edit before the button hears about it, so the
    // click that follows would fall through to the bin the button turns back
    // into.  Set when that happens, and swallowed by the delete handler.
    bool m_cancelClickPending = false;
    bool m_editIsNew = false; // a row that has no clock behind it yet
    bool m_editCommitted = false;
    bool m_populating = false;
    bool m_rebuildQueued = false;
};
