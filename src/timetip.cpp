#include "timetip.h"

#include <QAbstractTextDocumentLayout>
#include <QGuiApplication>
#include <QLocale>
#include <QPainter>
#include <QPainterPath>
#include <QScreen>
#include <QTextDocument>
#include <QTimer>
#include <QToolTip>
#include <QtMath>

namespace {

// Padding between the text and the edge of the bubble.
constexpr int kPadX = 10;
constexpr int kPadY = 7;
constexpr double kRadius = 6.0;

// How far the bubble sits from the pointer, chosen so it clears the usual
// cursor bitmap rather than hiding under it.
constexpr int kOffsetX = 16;
constexpr int kOffsetY = 20;

// Poll rather than firing exactly on the second: a timer that is meant to run
// once a second drifts, and the cost of noticing that nothing has changed is
// nothing.
constexpr int kPollMs = 200;

// 1st, 2nd, 3rd, 4th -- with the eleventh through thirteenth being the
// exceptions that the last digit alone would get wrong.
QString ordinalSuffix(int day)
{
    if (day >= 11 && day <= 13)
        return QStringLiteral("th");
    switch (day % 10) {
    case 1:
        return QStringLiteral("st");
    case 2:
        return QStringLiteral("nd");
    case 3:
        return QStringLiteral("rd");
    default:
        return QStringLiteral("th");
    }
}

}  // namespace

TimeTip::TimeTip(QWidget *parent)
    : QWidget(parent,
              Qt::ToolTip | Qt::FramelessWindowHint | Qt::WindowTransparentForInput
                  | Qt::WindowDoesNotAcceptFocus)
{
    setAttribute(Qt::WA_TranslucentBackground, true);
    setAttribute(Qt::WA_ShowWithoutActivating, true);
    // The bubble must never take the pointer: if it did, the clock underneath
    // would see the pointer leave and hide the very thing being pointed at.
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
    setFocusPolicy(Qt::NoFocus);

    m_doc = new QTextDocument(this);
    m_doc->setDefaultFont(QToolTip::font());
    m_doc->setDocumentMargin(0);

    m_tick = new QTimer(this);
    m_tick->setInterval(kPollMs);
    connect(m_tick, &QTimer::timeout, this, &TimeTip::refresh);
}

QString TimeTip::describe(const QDateTime &when)
{
    // The C locale rather than the system one, because the ordinal suffix
    // below is English and half-translating the line would read worse than
    // not translating it at all.
    const QLocale locale(QLocale::C);
    const QDate date = when.date();
    return QStringLiteral("<div align=\"center\" style=\"line-height:125%\">"
                          "%1<br>%2 %3<sup>%4</sup>, %5<br>%6</div>")
        .arg(locale.dayName(date.dayOfWeek(), QLocale::LongFormat),
             locale.monthName(date.month(), QLocale::LongFormat), QString::number(date.day()),
             ordinalSuffix(date.day()), QString::number(date.year()),
             locale.toString(when.time(), QStringLiteral("h:mm:ss AP")));
}

void TimeTip::popUp(const QPoint &globalPos)
{
    m_anchor = globalPos;
    m_text.clear();  // force a re-measure, in case the date grew a digit
    refresh();
    if (!isVisible())
        show();
    raise();
    m_tick->start();
}

void TimeTip::refresh()
{
    const QString text = describe(QDateTime::currentDateTime());
    if (text == m_text)
        return;
    m_text = text;

    m_doc->setHtml(m_text);
    // Measuring first and then fixing the width is what makes the centring
    // mean anything: without a width the lines have nothing to centre within.
    m_doc->setTextWidth(-1);
    m_doc->setTextWidth(m_doc->idealWidth());
    const QSizeF textSize = m_doc->size();

    const QSize wanted(qCeil(textSize.width()) + 2 * kPadX, qCeil(textSize.height()) + 2 * kPadY);
    if (wanted != size())
        resize(wanted);
    placeBeside(m_anchor);
    update();
}

// Nothing to keep up to date while the bubble is not on screen.
void TimeTip::hideEvent(QHideEvent *event)
{
    m_tick->stop();
    QWidget::hideEvent(event);
}

void TimeTip::placeBeside(const QPoint &globalPos)
{
    QScreen *screen = QGuiApplication::screenAt(globalPos);
    if (!screen)
        screen = QGuiApplication::primaryScreen();
    const QRect limits = screen ? screen->availableGeometry() : QRect();

    QPoint where(globalPos.x() + kOffsetX, globalPos.y() + kOffsetY);
    if (!limits.isNull()) {
        // Flip to the other side of the pointer rather than letting the bubble
        // run off the screen, and only then clamp.
        if (where.x() + width() > limits.right())
            where.setX(globalPos.x() - width() - kOffsetX / 2);
        if (where.y() + height() > limits.bottom())
            where.setY(globalPos.y() - height() - kOffsetY / 2);
        where.setX(qBound(limits.left(), where.x(), limits.right() - width()));
        where.setY(qBound(limits.top(), where.y(), limits.bottom() - height()));
    }
    move(where);
}

void TimeTip::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    const QPalette palette = QToolTip::palette();
    QPainterPath bubble;
    // Half a pixel in, so the one-pixel border lands on the pixel rather than
    // straddling two of them.
    bubble.addRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), kRadius, kRadius);
    painter.fillPath(bubble, palette.toolTipBase());
    painter.setPen(QPen(palette.toolTipText().color().lighter(180), 1.0));
    painter.drawPath(bubble);

    painter.translate(kPadX, kPadY);
    QAbstractTextDocumentLayout::PaintContext context;
    context.palette.setColor(QPalette::Text, palette.toolTipText().color());
    context.clip = QRectF(0, 0, m_doc->size().width(), m_doc->size().height());
    m_doc->documentLayout()->draw(&painter, context);
}
