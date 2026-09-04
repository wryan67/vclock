// The date and time bubble a clock shows while the pointer rests on it. An
// analog face tells you the time to the minute and says nothing at all about
// the date, so this fills in both.
#pragma once

#include <QDateTime>
#include <QPoint>
#include <QString>
#include <QWidget>

class QTextDocument;
class QTimer;

class TimeTip : public QWidget
{
    Q_OBJECT

public:
    explicit TimeTip(QWidget *parent = nullptr);

    // Put the bubble beside a point on the screen and keep its seconds
    // running until it is hidden again.
    void popUp(const QPoint &globalPos);

    // The three lines as they read at a given moment, as rich text. Kept
    // separate from the window so the wording can be checked on its own.
    static QString describe(const QDateTime &when);

protected:
    void paintEvent(QPaintEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private:
    void refresh();
    void placeBeside(const QPoint &globalPos);

    QString m_text;
    QPoint m_anchor;
    QTextDocument *m_doc = nullptr;
    QTimer *m_tick = nullptr;
};
