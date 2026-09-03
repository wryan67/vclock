#include "colorpicker.h"

#include <QDialogButtonBox>
#include <QGridLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QSpinBox>
#include <QVBoxLayout>

#include <cmath>

namespace {

constexpr qreal kRimWidth = 3.0;
constexpr int kWheelSize = 200;
constexpr int kSliderHeight = 26;
constexpr int kSwatchSize = 26;

// The wheel is drawn with red at the right and hue increasing anticlockwise,
// which is the arrangement of every wheel picker users are likely to have met.
qreal hueAt(qreal dx, qreal dy)
{
    qreal hue = std::atan2(dy, dx) * 180.0 / M_PI;
    if (hue < 0)
        hue += 360.0;
    return hue;
}

QColor fromHsv(int h, int s, int v)
{
    return QColor::fromHsv(qBound(0, h, 359), qBound(0, s, 255), qBound(0, v, 255));
}

// A ring that reads against both light and dark colours.
void drawHandle(QPainter &painter, const QPointF &pos, qreal r)
{
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(QColor(0, 0, 0, 160), 3.0));
    painter.drawEllipse(pos, r, r);
    painter.setPen(QPen(Qt::white, 2.0));
    painter.drawEllipse(pos, r, r);
}

}  // namespace

// ---------------------------------------------------------------- ColorWheel

ColorWheel::ColorWheel(QWidget *parent)
    : QWidget(parent)
{
    setCursor(Qt::CrossCursor);
    setFocusPolicy(Qt::ClickFocus);
}

QSize ColorWheel::sizeHint() const
{
    return QSize(kWheelSize, kWheelSize);
}

QPointF ColorWheel::centre() const
{
    return QPointF(width() / 2.0, height() / 2.0);
}

qreal ColorWheel::radius() const
{
    // Two pixels of margin, so the rim can be drawn kRimWidth wide without
    // half of it falling off the edge of the widget.
    return qMin(width(), height()) / 2.0 - 2.0;
}

void ColorWheel::setHsv(int h, int s, int v)
{
    m_hue = qBound(0, h, 359);
    m_sat = qBound(0, s, 255);
    m_val = qBound(0, v, 255);
    update();
}

void ColorWheel::resizeEvent(QResizeEvent *event)
{
    m_cacheVal = -1;
    QWidget::resizeEvent(event);
}

void ColorWheel::rebuildCache()
{
    const int side = qMin(width(), height());
    if (side <= 2) {
        m_cache = QImage();
        return;
    }
    const qreal dpr = devicePixelRatioF();
    const int px = qMax(2, int(side * dpr));
    QImage img(px, px, QImage::Format_ARGB32);
    img.fill(Qt::transparent);

    const qreal c = px / 2.0;
    const qreal rad = c - 2.0 * dpr;
    const qreal v = renderValue() / 255.0;
    for (int y = 0; y < px; ++y) {
        QRgb *line = reinterpret_cast<QRgb *>(img.scanLine(y));
        const qreal dy = c - (y + 0.5);
        for (int x = 0; x < px; ++x) {
            const qreal dx = (x + 0.5) - c;
            const qreal dist = std::hypot(dx, dy);
            if (dist > rad + dpr)
                continue;
            const qreal sat = qMin(1.0, dist / rad);
            const QColor col = QColor::fromHsvF(qMin(hueAt(dx, dy) / 360.0, 0.99999), sat, v);
            // Feather the rim rather than leaving it jagged.
            const qreal cover = dist <= rad ? 1.0 : qBound(0.0, 1.0 - (dist - rad) / dpr, 1.0);
            line[x] = qRgba(col.red(), col.green(), col.blue(), int(cover * 255.0 + 0.5));
        }
    }
    img.setDevicePixelRatio(dpr);
    m_cache = img;
    m_cacheVal = renderValue();
}

// The wheel is tinted by the current brightness so the colour is shown in
// context, but it is never allowed to go so dark that the hues stop being
// distinguishable -- the default hand colour is near-black, so without this
// floor the wheel would open unusable on most of this app's colour buttons.
int ColorWheel::renderValue() const
{
    return qMax(m_val, 140);
}

void ColorWheel::paintEvent(QPaintEvent *)
{
    if (m_cache.isNull() || m_cacheVal != renderValue())
        rebuildCache();

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    if (!m_cache.isNull()) {
        // The cache is square, the widget need not be: blit it about the same
        // centre the rim and the handle are measured from, or the two drift
        // apart as soon as the widget is not square.
        const QSizeF size(m_cache.size() / m_cache.devicePixelRatio());
        painter.drawImage(QPointF((width() - size.width()) / 2.0,
                                  (height() - size.height()) / 2.0),
                          m_cache);
    }

    painter.setPen(QPen(QColor(0, 0, 0, 150), kRimWidth));
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(centre(), radius(), radius());

    drawHandle(painter, handlePos(), 7.0);
}

QPointF ColorWheel::handlePos() const
{
    const qreal angle = m_hue * M_PI / 180.0;
    const qreal dist = (m_sat / 255.0) * radius();
    const QPointF c = centre();
    return QPointF(c.x() + std::cos(angle) * dist, c.y() - std::sin(angle) * dist);
}

void ColorWheel::pickFrom(const QPoint &pos)
{
    const QPointF c = centre();
    const qreal dx = pos.x() - c.x();
    const qreal dy = c.y() - pos.y();
    const qreal rad = radius();
    if (rad <= 0)
        return;
    m_hue = int(hueAt(dx, dy)) % 360;
    // Dragging past the rim keeps tracking the angle at full saturation, which
    // is what makes a wheel comfortable to use.
    m_sat = int(qBound(0.0, std::hypot(dx, dy) / rad, 1.0) * 255.0 + 0.5);
    update();
    emit hsvChanged(m_hue, m_sat, m_val);
}

void ColorWheel::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }
    pickFrom(event->pos());
    event->accept();
}

void ColorWheel::mouseMoveEvent(QMouseEvent *event)
{
    if (!(event->buttons() & Qt::LeftButton)) {
        QWidget::mouseMoveEvent(event);
        return;
    }
    pickFrom(event->pos());
    event->accept();
}

// ------------------------------------------------------------- ChannelSlider

ChannelSlider::ChannelSlider(Channel channel, QWidget *parent)
    : QWidget(parent)
    , m_channel(channel)
{
    setCursor(Qt::PointingHandCursor);
    setToolTip(channel == Saturation ? QStringLiteral("Saturation — the wheel's centre-to-rim axis")
                                     : QStringLiteral("Brightness"));
}

QSize ChannelSlider::sizeHint() const
{
    return QSize(kWheelSize, kSliderHeight);
}

qreal ChannelSlider::handleRadius() const
{
    return height() / 2.0 - 2.0;
}

void ChannelSlider::setHsv(int h, int s, int v)
{
    m_hue = qBound(0, h, 359);
    m_sat = qBound(0, s, 255);
    m_val = qBound(0, v, 255);
    update();
}

void ChannelSlider::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const qreal r = height() / 2.0;
    const QRectF track(0.5, 0.5, width() - 1.0, height() - 1.0);
    QLinearGradient grad(track.left(), 0, track.right(), 0);
    // Each bar is drawn with the other two channels held where they are, so it
    // shows the range it will actually move through.
    if (m_channel == Saturation) {
        grad.setColorAt(0.0, fromHsv(m_hue, 0, m_val));
        grad.setColorAt(1.0, fromHsv(m_hue, 255, m_val));
    } else {
        grad.setColorAt(0.0, Qt::black);
        grad.setColorAt(1.0, fromHsv(m_hue, m_sat, 255));
    }

    QPainterPath path;
    path.addRoundedRect(track, r, r);
    painter.fillPath(path, grad);
    painter.setPen(QPen(QColor(0, 0, 0, 60), 1.0));
    painter.drawPath(path);

    const qreal hr = handleRadius();
    const qreal usable = width() - 2.0 * (hr + 2.0);
    const qreal x = hr + 2.0 + (channelValue() / 255.0) * usable;
    drawHandle(painter, QPointF(x, height() / 2.0), hr - 1.0);
}

void ChannelSlider::pickFrom(const QPoint &pos)
{
    const qreal hr = handleRadius();
    const qreal usable = width() - 2.0 * (hr + 2.0);
    if (usable <= 0)
        return;
    const int level = int(qBound(0.0, (pos.x() - hr - 2.0) / usable, 1.0) * 255.0 + 0.5);
    if (m_channel == Saturation)
        m_sat = level;
    else
        m_val = level;
    update();
    emit hsvChanged(m_hue, m_sat, m_val);
}

void ChannelSlider::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }
    pickFrom(event->pos());
    event->accept();
}

void ChannelSlider::mouseMoveEvent(QMouseEvent *event)
{
    if (!(event->buttons() & Qt::LeftButton)) {
        QWidget::mouseMoveEvent(event);
        return;
    }
    pickFrom(event->pos());
    event->accept();
}

// -------------------------------------------------------------- ColorPreview

ColorPreview::ColorPreview(const QColor &before, QWidget *parent)
    : QWidget(parent)
    , m_before(before)
    , m_current(before)
{
    setToolTip(QStringLiteral("Left half: the colour you started with — click it to go back"));
}

QSize ColorPreview::sizeHint() const
{
    return QSize(120, 34);
}

void ColorPreview::setColor(const QColor &color)
{
    m_current = color;
    update();
}

void ColorPreview::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    const QRect r = rect().adjusted(0, 0, -1, -1);
    const int half = r.width() / 2;
    painter.fillRect(QRect(r.left(), r.top(), half, r.height()), m_before);
    painter.fillRect(QRect(r.left() + half, r.top(), r.width() - half, r.height()), m_current);
    painter.setPen(QColor(0, 0, 0, 110));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(r);
}

void ColorPreview::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && event->pos().x() < width() / 2) {
        emit revertRequested();
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

// ---------------------------------------------------------- ColorPickerDialog

// --------------------------------------------------------------- SwatchGrid

namespace {

constexpr int kSwatchGap = 4;

}  // namespace

SwatchGrid::SwatchGrid(QWidget *parent)
    : QWidget(parent)
{
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setCursor(Qt::PointingHandCursor);
}

QColor SwatchGrid::colorAt(int row, int col) const
{
    // Eight columns of one hue each plus a greyscale column.  Pink and teal
    // earn their place because neither is reachable by shading a neighbour:
    // pink is a tint rather than a shade of red, and the gap between green and
    // blue is wide enough that teal is a long way from either.
    static const int kHues[] = {0, 28, 50, 120, 172, 212, 275, 322};
    constexpr int kHueColumns = int(sizeof(kHues) / sizeof(kHues[0]));

    // Every column runs light to dark.  The top two rows are tints -- the same
    // hue washed out towards white, which is where the pinks and lavenders
    // live -- then the pure colour, then shades down towards black.  Keeping
    // one ramp per column means a row reads as a single weight the whole way
    // across, which a hand-written list never quite manages.
    struct Tone
    {
        int sat;
        int val;
    };
    static const Tone kTones[kRows] = {{55, 255},  {115, 255}, {255, 255},
                                       {255, 217}, {255, 179}, {255, 143},
                                       {255, 110}, {255, 79},  {255, 51}};

    if (col < kHueColumns)
        return QColor::fromHsv(kHues[col], kTones[row].sat, kTones[row].val);

    // The greys follow the same light-to-dark run, white at the top and black
    // at the bottom, so the last column reads as part of the same ramp rather
    // than against it.
    const int level = int(255.0 * (kRows - 1 - row) / (kRows - 1) + 0.5);
    return QColor(level, level, level);
}

QSize SwatchGrid::sizeHint() const
{
    const int pitch = kSwatchSize + kSwatchGap;
    return QSize(kColumns * pitch - kSwatchGap, kRows * pitch - kSwatchGap);
}

QRect SwatchGrid::cellRect(int row, int col) const
{
    const int pitch = kSwatchSize + kSwatchGap;
    return QRect(col * pitch, row * pitch, kSwatchSize, kSwatchSize);
}

QPoint SwatchGrid::cellAt(const QPoint &pos) const
{
    const int pitch = kSwatchSize + kSwatchGap;
    const int col = pos.x() / pitch;
    const int row = pos.y() / pitch;
    if (row < 0 || row >= kRows || col < 0 || col >= kColumns)
        return QPoint(-1, -1);
    // The gap between swatches belongs to neither of them.
    if (!cellRect(row, col).contains(pos))
        return QPoint(-1, -1);
    return QPoint(col, row);
}

void SwatchGrid::setCurrentColor(const QColor &color)
{
    if (m_current == color)
        return;
    m_current = color;
    update();
}

void SwatchGrid::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    for (int row = 0; row < kRows; ++row) {
        for (int col = 0; col < kColumns; ++col) {
            const QColor c = colorAt(row, col);
            const QRectF r(cellRect(row, col));
            const bool chosen = m_current.isValid() && c.rgb() == m_current.rgb();

            // The chosen swatch draws smaller, inside a white ring. The ring is
            // fenced with hairlines on both sides, because white alone is
            // invisible against the white swatch and against the dialog.
            const qreal inset = chosen ? 4.5 : 0.5;
            painter.setBrush(c);
            painter.setPen(QPen(QColor(0, 0, 0, 70), 1.0));
            painter.drawEllipse(r.adjusted(inset, inset, -inset, -inset));

            if (chosen) {
                painter.setBrush(Qt::NoBrush);
                painter.setPen(QPen(Qt::white, 3.0));
                painter.drawEllipse(r.adjusted(2.5, 2.5, -2.5, -2.5));
                painter.setPen(QPen(QColor(0, 0, 0, 90), 1.0));
                painter.drawEllipse(r.adjusted(0.5, 0.5, -0.5, -0.5));
            }

            // There is no separate keyboard cursor: arrowing chooses as it
            // moves, so the ring above always marks where the keys are.
            if (!chosen && row == m_hoverRow && col == m_hoverCol) {
                painter.setBrush(Qt::NoBrush);
                painter.setPen(QPen(QColor(0x42, 0x99, 0xe1), 2.0));
                painter.drawEllipse(r.adjusted(1.0, 1.0, -1.0, -1.0));
            }
        }
    }
}

void SwatchGrid::mousePressEvent(QMouseEvent *event)
{
    const QPoint cell = cellAt(event->pos());
    if (cell.x() < 0) {
        QWidget::mousePressEvent(event);
        return;
    }
    // Clicking is also where the keyboard starts from, so the arrow keys
    // continue from the swatch just chosen rather than from a corner.
    m_cursorCol = cell.x();
    m_cursorRow = cell.y();
    setFocus(Qt::MouseFocusReason);
    emit colorPicked(colorAt(m_cursorRow, m_cursorCol));
    update();
}

void SwatchGrid::mouseMoveEvent(QMouseEvent *event)
{
    const QPoint cell = cellAt(event->pos());
    if (cell.y() == m_hoverRow && cell.x() == m_hoverCol)
        return;
    m_hoverCol = cell.x();
    m_hoverRow = cell.y();
    update();
}

void SwatchGrid::leaveEvent(QEvent *event)
{
    m_hoverRow = m_hoverCol = -1;
    update();
    QWidget::leaveEvent(event);
}

void SwatchGrid::focusOutEvent(QFocusEvent *event)
{
    update();  // the cursor is only drawn while the grid has focus
    QWidget::focusOutEvent(event);
}

void SwatchGrid::keyPressEvent(QKeyEvent *event)
{
    int dRow = 0, dCol = 0;
    switch (event->key()) {
    case Qt::Key_Left:
        dCol = -1;
        break;
    case Qt::Key_Right:
        dCol = 1;
        break;
    case Qt::Key_Up:
        dRow = -1;
        break;
    case Qt::Key_Down:
        dRow = 1;
        break;
    case Qt::Key_Space:
    case Qt::Key_Return:
    case Qt::Key_Enter:
        if (m_cursorRow >= 0)
            emit colorPicked(colorAt(m_cursorRow, m_cursorCol));
        return;
    default:
        QWidget::keyPressEvent(event);
        return;
    }

    if (m_cursorRow < 0) {
        // Tabbed in rather than clicked: start on the chosen swatch when one of
        // them is chosen, and at the top left when none is.
        m_cursorRow = m_cursorCol = 0;
        bool found = false;
        for (int row = 0; row < kRows && !found; ++row) {
            for (int col = 0; col < kColumns; ++col) {
                if (m_current.isValid() && colorAt(row, col).rgb() == m_current.rgb()) {
                    m_cursorRow = row;
                    m_cursorCol = col;
                    found = true;
                    break;
                }
            }
        }
    } else {
        m_cursorRow = qBound(0, m_cursorRow + dRow, kRows - 1);
        m_cursorCol = qBound(0, m_cursorCol + dCol, kColumns - 1);
    }
    update();
    // Arrowing is choosing: the colour follows the cursor, so there is nothing
    // left for space to confirm.
    emit colorPicked(colorAt(m_cursorRow, m_cursorCol));
}

ColorPickerDialog::ColorPickerDialog(const QColor &initial, QWidget *parent)
    : QDialog(parent)
    , m_color(initial.isValid() ? initial : QColor(Qt::black))
{
    auto *outer = new QVBoxLayout(this);
    auto *top = new QHBoxLayout;
    outer->addLayout(top);

    auto *left = new QVBoxLayout;
    m_wheel = new ColorWheel(this);
    m_satSlider = new ChannelSlider(ChannelSlider::Saturation, this);
    m_valSlider = new ChannelSlider(ChannelSlider::Value, this);
    left->addWidget(m_wheel);
    auto *sliders = new QGridLayout;
    sliders->setSpacing(4);
    sliders->addWidget(new QLabel(QStringLiteral("S"), this), 0, 0);
    sliders->addWidget(m_satSlider, 0, 1);
    sliders->addWidget(new QLabel(QStringLiteral("B"), this), 1, 0);
    sliders->addWidget(m_valSlider, 1, 1);
    left->addLayout(sliders);
    top->addLayout(left);

    auto *right = new QVBoxLayout;
    m_swatches = new SwatchGrid(this);
    connect(m_swatches, &SwatchGrid::colorPicked, this,
            [this](const QColor &c) { applyColor(c, m_swatches); });
    right->addWidget(m_swatches, 0, Qt::AlignTop | Qt::AlignLeft);
    right->addStretch(1);
    top->addLayout(right);

    auto *fields = new QGridLayout;
    fields->addWidget(new QLabel(QStringLiteral("HTML"), this), 0, 0);
    m_hex = new QLineEdit(this);
    m_hex->setMaxLength(7);
    // Accepts with or without the leading hash, and tolerates a partly typed
    // value so the field can be edited normally.
    m_hex->setValidator(new QRegularExpressionValidator(
        QRegularExpression(QStringLiteral("^#?[0-9A-Fa-f]{0,6}$")), m_hex));
    m_hex->setFixedWidth(90);
    fields->addWidget(m_hex, 0, 1, Qt::AlignLeft);

    fields->addWidget(new QLabel(QStringLiteral("RGB"), this), 1, 0);
    auto *rgbRow = new QHBoxLayout;
    rgbRow->setSpacing(4);
    for (int i = 0; i < 3; ++i) {
        m_rgb[i] = new QSpinBox(this);
        m_rgb[i]->setRange(0, 255);
        m_rgb[i]->setFixedWidth(58);
        m_rgb[i]->setToolTip(QStringList{QStringLiteral("Red"), QStringLiteral("Green"),
                                         QStringLiteral("Blue")}
                                 .at(i));
        rgbRow->addWidget(m_rgb[i]);
    }
    rgbRow->addStretch(1);
    fields->addLayout(rgbRow, 1, 1, Qt::AlignLeft);

    m_preview = new ColorPreview(m_color, this);
    fields->addWidget(m_preview, 0, 2, 2, 1);
    fields->setColumnStretch(2, 1);
    outer->addLayout(fields);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    outer->addWidget(buttons);

    connect(m_wheel, &ColorWheel::hsvChanged, this,
            [this](int h, int s, int v) { applyHsv(h, s, v, m_wheel); });
    connect(m_satSlider, &ChannelSlider::hsvChanged, this,
            [this](int h, int s, int v) { applyHsv(h, s, v, m_satSlider); });
    connect(m_valSlider, &ChannelSlider::hsvChanged, this,
            [this](int h, int s, int v) { applyHsv(h, s, v, m_valSlider); });
    connect(m_preview, &ColorPreview::revertRequested, this,
            [this, initial] { applyColor(initial, nullptr); });
    connect(m_hex, &QLineEdit::textEdited, this, [this](const QString &text) {
        QString body = text;
        if (body.startsWith(QLatin1Char('#')))
            body.remove(0, 1);
        // Only a complete value means anything; half-typed text is left alone
        // so the field does not fight the person using it.
        if (body.size() != 6)
            return;
        const QColor c(QLatin1Char('#') + body);
        if (c.isValid())
            applyColor(c, m_hex);
    });
    // Tidy a partial or empty entry back to the colour actually in force.
    connect(m_hex, &QLineEdit::editingFinished, this,
            [this] { m_hex->setText(m_color.name(QColor::HexRgb)); });
    for (int i = 0; i < 3; ++i) {
        connect(m_rgb[i], &QSpinBox::valueChanged, this, [this] {
            if (m_updating)
                return;
            applyColor(QColor(m_rgb[0]->value(), m_rgb[1]->value(), m_rgb[2]->value()), m_rgb[0]);
        });
    }
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    applyColor(m_color, nullptr);
}


void ColorPickerDialog::applyColor(const QColor &color, QWidget *source)
{
    if (!color.isValid() || m_updating)
        return;
    const QColor rgb(color.red(), color.green(), color.blue());
    // Hold on to the hue and saturation the colour cannot carry: a grey has no
    // hue, and black has neither, so taking either slider to zero would
    // otherwise throw away where the wheel handle was.
    const int h = rgb.hue() >= 0 ? rgb.hue() : m_hue;
    const int s = rgb.value() > 0 ? rgb.saturation() : m_sat;
    applyHsv(h, s, rgb.value(), source, rgb);
}

void ColorPickerDialog::applyHsv(int h, int s, int v, QWidget *source, const QColor &exact)
{
    if (m_updating)
        return;
    m_updating = true;
    m_hue = qBound(0, h, 359);
    m_sat = qBound(0, s, 255);
    m_val = qBound(0, v, 255);
    // A quarter of the preset swatches do not survive the trip out to HSV and
    // back -- and the drift compounds, so repeated applies walk the colour
    // away from where it started.  When the caller named an RGB value, that
    // value is the colour; the HSV is only how the wheel and sliders show it.
    m_color = exact.isValid() ? exact.toRgb() : fromHsv(m_hue, m_sat, m_val).toRgb();

    // Every control but the one being used is refreshed.  Both sliders are
    // redrawn whenever the other one moves, because each bar's gradient is
    // painted at the other channels' current values.
    if (source != m_wheel)
        m_wheel->setHsv(m_hue, m_sat, m_val);
    if (source != m_satSlider)
        m_satSlider->setHsv(m_hue, m_sat, m_val);
    if (source != m_valSlider)
        m_valSlider->setHsv(m_hue, m_sat, m_val);
    if (source != m_hex)
        m_hex->setText(m_color.name(QColor::HexRgb));
    // m_rgb[0] stands for the whole row: whichever of the three was edited,
    // all of them already hold the values that produced this colour.
    if (source != m_rgb[0]) {
        m_rgb[0]->setValue(m_color.red());
        m_rgb[1]->setValue(m_color.green());
        m_rgb[2]->setValue(m_color.blue());
    }
    m_preview->setColor(m_color);
    // Not skipped for the grid itself: a click there marks the swatch, and
    // arriving at the same colour any other way should mark it too.
    m_swatches->setCurrentColor(m_color);

    m_updating = false;
    emit currentColorChanged(m_color);
}

void ColorPickerDialog::setCurrentColor(const QColor &color)
{
    applyColor(color, nullptr);
}
