#pragma once

#include <QTreeView>

namespace Acheron {
namespace UI {

class ChannelTreeView : public QTreeView
{
    Q_OBJECT
public:
    ChannelTreeView(QWidget *parent = nullptr);

private:
    // surely theres a simpler way
    void mousePressEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;

    void handleMouseEventForExpansion(QMouseEvent *event);
};

} // namespace UI
} // namespace Acheron