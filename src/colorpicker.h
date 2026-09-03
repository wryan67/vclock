// A colour picker in the style of the wheel-and-slider pickers common on the
// web: a hue/saturation wheel, a brightness slider under it, a grid of preset
// swatches, and an HTML hex field.
//
// Qt's own QColorDialog is capable but cluttered, and its "basic colors" grid
// is a poor fit for choosing clock colours.  This is a native reimplementation
// of that simpler layout -- no third-party code is used -- extended with the
// hex field, a greyscale row and a before/after preview, which the plain
// version lacks.
//
// The dialog mirrors the parts of QColorDialog's API that ColorButton relies
// on, so it can be swapped in directly: currentColor(), setCurrentColor() and
// currentColorChanged() for the live preview.
#pragma once

#include <QColor>
#include <QDialog>
#include <QImage>
#include <QWidget>

class QLineEdit;
class QSpinBox;

// The hue/saturation disc.  Hue runs anticlockwise from red at the right, and
// saturation grows from the centre outwards; brightness comes from the slider
// and tints the whole wheel so it always previews the colour in context.
class ColorWheel : public QWidget
{
    Q_OBJECT

public:
    explicit ColorWheel(QWidget *parent = nullptr);

    QSize sizeHint() const override;
    void setHsv(int h, int s, int v);

signals:
    void hsvChanged(int h, int s, int v);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void rebuildCache();
    int renderValue() const;
    void pickFrom(const QPoint &pos);
    QPointF handlePos() const;
    qreal radius() const;
    QPointF centre() const;

    int m_hue = 0;       // 0-359, kept across greys so the wheel handle stays put
    int m_sat = 0;       // 0-255
    int m_val = 255;     // 0-255
    QImage m_cache;      // the wheel at the current brightness
    int m_cacheVal = -1; // brightness the cache was built for
};

// A horizontal bar for one HSV channel.  Saturation runs grey to full colour,
// brightness runs black to fully lit; both are drawn at the wheel's current
// hue so the bar always previews what it will actually give you.
//
// Saturation is the centre-to-rim axis of the wheel, so this slider and the
// wheel's handle move together -- the slider just makes that axis adjustable
// on its own, without disturbing the hue.
class ChannelSlider : public QWidget
{
    Q_OBJECT

public:
    enum Channel { Saturation, Value };

    explicit ChannelSlider(Channel channel, QWidget *parent = nullptr);

    QSize sizeHint() const override;
    void setHsv(int h, int s, int v);

signals:
    void hsvChanged(int h, int s, int v);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;

private:
    void pickFrom(const QPoint &pos);
    qreal handleRadius() const;
    int channelValue() const { return m_channel == Saturation ? m_sat : m_val; }

    Channel m_channel;
    int m_hue = 0;
    int m_sat = 0;
    int m_val = 255;
};

// Shows the colour the dialog opened on beside the colour now chosen; clicking
// the "before" half puts the original colour back.
class ColorPreview : public QWidget
{
    Q_OBJECT

public:
    explicit ColorPreview(const QColor &before, QWidget *parent = nullptr);

    QSize sizeHint() const override;
    void setColor(const QColor &color);

signals:
    void revertRequested();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

private:
    QColor m_before;
    QColor m_current;
};

// The grid of preset colours.  One widget rather than a grid of buttons: it
// paints the ring on whichever swatch matches the current colour, and moving
// between swatches with the arrow keys is navigation within one control rather
// than a walk through nine rows of the focus chain.
class SwatchGrid : public QWidget
{
    Q_OBJECT

public:
    explicit SwatchGrid(QWidget *parent = nullptr);

    // Ring the swatch matching this colour, if any of them do.
    void setCurrentColor(const QColor &color);

    QSize sizeHint() const override;

signals:
    void colorPicked(const QColor &color);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;

private:
    static constexpr int kColumns = 9;
    static constexpr int kRows = 9;

    QColor colorAt(int row, int col) const;
    QRect cellRect(int row, int col) const;
    QPoint cellAt(const QPoint &pos) const;  // (-1,-1) when the point is between cells

    QColor m_current;
    // Where the keyboard is. -1 until a swatch is clicked or the grid is
    // tabbed into, so arrow keys have somewhere to start from.
    int m_cursorRow = -1;
    int m_cursorCol = -1;
    int m_hoverRow = -1;
    int m_hoverCol = -1;
};

class ColorPickerDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ColorPickerDialog(const QColor &initial, QWidget *parent = nullptr);

    QColor currentColor() const { return m_color; }
    void setCurrentColor(const QColor &color);

signals:
    void currentColorChanged(const QColor &color);

private:
    // Applies a colour to every control and announces it.  source is the widget
    // that originated the change and is left alone, so that editing the hex
    // field does not fight the caret, and dragging a handle does not snap.
    void applyColor(const QColor &color, QWidget *source);
    // The dialog keeps hue, saturation and brightness itself rather than
    // reading them back off the chosen colour.  RGB cannot express them all:
    // black has no hue or saturation, and any grey has no hue, so a round trip
    // through a colour would forget where the wheel handle and the saturation
    // slider were as soon as either slider reached zero.
    void applyHsv(int h, int s, int v, QWidget *source);
    QColor m_color;
    int m_hue = 0;
    int m_sat = 0;
    int m_val = 0;
    ColorWheel *m_wheel = nullptr;
    ChannelSlider *m_satSlider = nullptr;
    ChannelSlider *m_valSlider = nullptr;
    ColorPreview *m_preview = nullptr;
    SwatchGrid *m_swatches = nullptr;
    QLineEdit *m_hex = nullptr;
    QSpinBox *m_rgb[3] = {nullptr, nullptr, nullptr};
    bool m_updating = false;
};
