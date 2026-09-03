// The list of clocks, with a name you can edit in place.
#pragma once

#include <QDialog>

class QPushButton;
class QTableWidget;
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
    bool editing() const { return m_editRow >= 0; }
    void setRowEditing(int row, bool on);

    void deleteRow(int row);
    // Open the settings for a row's clock, putting the clock on screen first if
    // it is hidden -- there is nothing to change the look of otherwise.
    void openRowSettings(int row);
    void toggleOpen(int row, bool open);

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

    // -1 when nothing is being edited.
    int m_editRow = -1;
    QString m_editWasNamed;   // the name to go back to on cancel
    bool m_editIsNew = false; // a row that has no clock behind it yet
    bool m_editCommitted = false;
    bool m_populating = false;
    bool m_rebuildQueued = false;
};
