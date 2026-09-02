// A colour swatch button that previews changes live.
//
// A plain QColorDialog only reports the colour once it is accepted, so the
// clock could not follow the colour while it was being chosen.  This drives a
// non-native QColorDialog and re-emits colorSet() on every currentColorChanged,
// giving a live preview; Cancel restores the colour that was in effect when the
// dialog opened.
#pragma once

#include <QColor>
#include <QPushButton>
#include <QString>

class QColorDialog;

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
    QColorDialog *m_dialog = nullptr;
};
