#include "colorbutton.h"

#include "colorpicker.h"

#include <QPainter>
#include <QStyleOptionButton>
#include <QStylePainter>

#include <utility>

namespace {
constexpr int kSwatchW = 48;
constexpr int kSwatchH = 22;
constexpr int kPadding = 6;

// How far apart the two checkerboard squares are.  Not as far as a light theme
// could take: the same step that reads as a tasteful grey checkerboard under
// white takes the dark squares to nearly black under a dark panel.
constexpr int kSwatchChecker = 35;

// The two square colours for the "this colour is see-through" checkerboard,
// worked out from whatever the pattern will be drawn on.  A hard-coded light
// grey is the obvious way to do this and the wrong one: on a dark theme it
// punches a glaring white hole in the panel.  These straddle the background
// instead -- one square lighter, one darker -- so the pattern reads as texture
// on the surface it sits on, under any theme.
std::pair<QColor, QColor> checkerShades(const QColor &background, int strength)
{
    const QColor base = background.isValid() ? background : QColor(204, 204, 204);
    strength = qBound(0, strength, 127);

    // Shift the background's lightness both ways.  Working in lightness rather
    // than with lighter()/darker() keeps the step the same size wherever the
    // background sits, and those two multiply -- so they do nothing at all on
    // black, which is exactly where a dark theme puts us.
    const int lightness = base.lightness();
    int high = qMin(255, lightness + strength);
    int low = qMax(0, lightness - strength);
    // Near black or near white one side has nowhere to go, so take the whole
    // step out of the other and keep the two squares as far apart as asked.
    if (high - lightness < strength)
        low = qMax(0, high - 2 * strength);
    if (lightness - low < strength)
        high = qMin(255, low + 2 * strength);

    const int hue = base.hslHue() < 0 ? 0 : base.hslHue();
    const int saturation = base.hslHue() < 0 ? 0 : base.hslSaturation();
    return {QColor::fromHsl(hue, saturation, high), QColor::fromHsl(hue, saturation, low)};
}
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
        // Checkerboard behind partially clear colours, in the theme's own
        // shades so it is a pattern on the button rather than a hole in it.
        const auto [light, dark] = checkerShades(palette().color(QPalette::Button),
                                                 kSwatchChecker);
        const int step = 6;
        painter.setPen(Qt::NoPen);
        for (int iy = 0; iy < swatch.height(); iy += step) {
            for (int ix = 0; ix < swatch.width(); ix += step) {
                painter.fillRect(QRect(swatch.left() + ix, swatch.top() + iy,
                                       qMin(step, swatch.width() - ix),
                                       qMin(step, swatch.height() - iy)),
                                 ((ix / step) + (iy / step)) % 2 ? dark : light);
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
