#include "colorbutton.h"

#include "colorpicker.h"

#include <QPainter>
#include <QStyleOptionButton>
#include <QStylePainter>

namespace {
constexpr int kSwatchW = 48;
constexpr int kSwatchH = 22;
constexpr int kPadding = 6;
}  // namespace

ColorButton::ColorButton(const QColor &color, bool useAlpha, const QString &title,
                         QWidget *parent)
    : QPushButton(parent)
    , m_color(color.isValid() ? color : QColor(Qt::black))
    , m_useAlpha(useAlpha)
    , m_title(title)
{
    setToolTip(title);
    connect(this, &QPushButton::clicked, this, &ColorButton::openPicker);
}

QSize ColorButton::sizeHint() const
{
    return QSize(kSwatchW + 2 * kPadding, kSwatchH + 2 * kPadding);
}

void ColorButton::setColor(const QColor &color)
{
    if (!color.isValid())
        return;
    m_color = color;
    update();
}

void ColorButton::setUseAlpha(bool value)
{
    m_useAlpha = value;
    update();
}

void ColorButton::paintEvent(QPaintEvent *)
{
    QStylePainter painter(this);
    QStyleOptionButton option;
    initStyleOption(&option);
    painter.drawControl(QStyle::CE_PushButton, option);

    QRect swatch = rect().adjusted(kPadding, kPadding, -kPadding, -kPadding);
    if (swatch.width() <= 0 || swatch.height() <= 0)
        return;
    if (!isEnabled())
        painter.setOpacity(0.5);

    const int alpha = m_useAlpha ? m_color.alpha() : 255;
    if (alpha < 255) {
        // Standard light/dark checkerboard behind partially clear colours.
        const int step = 6;
        painter.setPen(Qt::NoPen);
        for (int iy = 0; iy < swatch.height(); iy += step) {
            for (int ix = 0; ix < swatch.width(); ix += step) {
                const int shade = ((ix / step) + (iy / step)) % 2 ? 153 : 255;
                painter.fillRect(QRect(swatch.left() + ix, swatch.top() + iy,
                                       qMin(step, swatch.width() - ix),
                                       qMin(step, swatch.height() - iy)),
                                 QColor(shade, shade, shade));
            }
        }
    }

    QColor fill = m_color;
    fill.setAlpha(alpha);
    painter.fillRect(swatch, fill);
    painter.setPen(QColor(0, 0, 0, 115));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(swatch.adjusted(0, 0, -1, -1));
}

void ColorButton::openPicker()
{
    if (m_dialog) {  // already open; just raise it
        m_dialog->raise();
        m_dialog->activateWindow();
        return;
    }

    const QColor before = m_color;
    auto *dialog = new ColorPickerDialog(m_color, window());
    dialog->setWindowTitle(m_title);
    dialog->setAttribute(Qt::WA_DeleteOnClose, true);
    dialog->setModal(true);

    connect(dialog, &ColorPickerDialog::currentColorChanged, this, [this](const QColor &c) {
        if (!c.isValid() || c.rgb() == m_color.rgb())
            return;
        m_color = QColor(c.red(), c.green(), c.blue(), m_color.alpha());
        update();
        emit colorSet();
    });
    connect(dialog, &QDialog::finished, this, [this, before](int result) {
        if (result != QDialog::Accepted && m_color.rgb() != before.rgb()) {
            m_color = before;  // Cancel: put the old colour back
            update();
            emit colorSet();
        }
        m_dialog = nullptr;
    });

    m_dialog = dialog;
    dialog->open();
}
