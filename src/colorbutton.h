// A colour swatch button that previews changes live.
//
// A plain QColorDialog only reports the colour once it is accepted, so the
// clock could not follow the colour while it was being chosen.  This drives
// ColorPickerDialog and re-emits colorSet() on every currentColorChanged,
// giving a live preview; Cancel restores the colour that was in effect when the
// dialog opened.
#pragma once

#include <QColor>
#include <QPushButton>
#include <QString>

#include <utility>

class ColorPickerDialog;

// The two square colours for a "this part is see-through" checkerboard, worked
// out from whatever the pattern will be drawn on.  Hard-coded light greys are
// the obvious way to do this and the wrong one: on a dark theme they punch a
// glaring white hole in the panel.  These straddle the background instead --
// one square lighter, one darker -- so the pattern reads as texture on the
// surface it sits on, under any theme.  strength is how far apart the two
// squares are, 0-127; small is subtle.
std::pair<QColor, QColor> checkerShades(const QColor &background, int strength);

class ColorButton : public QPushButton
{
    Q_OBJECT

public:
    explicit ColorButton(const QColor &color = QColor(Qt::black), bool useAlpha = false,
                         const QString &title = QStringLiteral("Choose a color"),
                         QWidget *parent = nullptr);

    QColor color() const { return m_color; }
    // Set the colour without emitting colorSet(), so mirroring one swatch into
    // another cannot recurse.
    void setColor(const QColor &color);

    bool useAlpha() const { return m_useAlpha; }
    void setUseAlpha(bool value);

    QSize sizeHint() const override;

signals:
    void colorSet();

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    void openPicker();

    QColor m_color;
    bool m_useAlpha = false;
    QString m_title;
    ColorPickerDialog *m_dialog = nullptr;
};
