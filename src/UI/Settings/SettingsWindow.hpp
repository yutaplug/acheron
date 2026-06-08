#pragma once

#include <QtWidgets>

namespace Acheron {
namespace UI {

class SettingsWindow : public QWidget
{
    Q_OBJECT
public:
    explicit SettingsWindow(QWidget *parent = nullptr);

private:
    void setupUi();

    QListWidget *categoryList;
    QStackedWidget *pages;
};

} // namespace UI
} // namespace Acheron
