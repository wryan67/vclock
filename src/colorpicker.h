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
    void setColor(const QColor &color);

signals:
    void colorChanged(const QColor &color);

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

// The brightness bar: black at the left, the wheel's fully lit colour at the
// right.
class BrightnessSlider : public QWidget
{
    Q_OBJECT

public:
    explicit BrightnessSlider(QWidget *parent = nullptr);

    QSize sizeHint() const override;
    void setColor(const QColor &color);

signals:
    void colorChanged(const QColor &color);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;

private:
    void pickFrom(const QPoint &pos);
    qreal handleRadius() const;

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
    void buildSwatches(class QGridLayout *grid);
    void addSwatch(class QGridLayout *grid, const QColor &c, int row, int col);

    static constexpr int kSwatchColumns = 7;
    static constexpr int kSwatchRows = 7;

    QColor m_color;
    ColorWheel *m_wheel = nullptr;
    BrightnessSlider *m_slider = nullptr;
    ColorPreview *m_preview = nullptr;
    QLineEdit *m_hex = nullptr;
    QSpinBox *m_rgb[3] = {nullptr, nullptr, nullptr};
    bool m_updating = false;
};
