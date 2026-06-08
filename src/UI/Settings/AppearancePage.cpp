#include "AppearancePage.hpp"

#include "Core/Theme/Generator.hpp"
#include "Core/Theme/Manager.hpp"
#include "Core/Theme/Tokens.hpp"

#include <QColorDialog>
#include <QComboBox>
#include <QFileDialog>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QRandomGenerator>
#include <QScrollArea>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>

namespace Acheron {
namespace UI {

using Core::Theme::Manager;
using Core::Theme::Token;
using Core::Theme::TokenDescriptor;

namespace {
QString swatchStyle(const QColor &c)
{
    return QStringLiteral(
                   "QPushButton { background-color: %1; border: 1px solid #888888; "
                   "border-radius: 3px; }")
            .arg(c.name(QColor::HexRgb));
}
} // namespace

AppearancePage::AppearancePage(QWidget *parent)
    : QWidget(parent)
{
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);

    seedColor = Manager::instance().color(Token::Highlight);

    auto *genGroup = new QGroupBox(tr("Generate from a color"), this);
    auto *genLayout = new QHBoxLayout(genGroup);

    genLayout->addWidget(new QLabel(tr("Base:"), genGroup));

    seedSwatch = new QPushButton(genGroup);
    seedSwatch->setFixedSize(48, 22);
    seedSwatch->setCursor(Qt::PointingHandCursor);
    seedSwatch->setStyleSheet(swatchStyle(seedColor));
    genLayout->addWidget(seedSwatch);

    auto *schemeCombo = new QComboBox(genGroup);
    for (int i = 0; i < Core::Theme::schemeCount; ++i)
        schemeCombo->addItem(Core::Theme::schemeName(static_cast<Core::Theme::Scheme>(i)));
    genLayout->addWidget(schemeCombo);

    auto *modeCombo = new QComboBox(genGroup);
    modeCombo->addItem(tr("Dark"));
    modeCombo->addItem(tr("Light"));
    genLayout->addWidget(modeCombo);

    auto *genBtn = new QPushButton(tr("Generate"), genGroup);
    auto *randBtn = new QPushButton(tr("Randomize"), genGroup);
    genLayout->addWidget(genBtn);
    genLayout->addWidget(randBtn);
    genLayout->addStretch(1);

    outer->addWidget(genGroup);

    connect(seedSwatch, &QPushButton::clicked, this, [this]() {
        const QColor picked = QColorDialog::getColor(seedColor, this, tr("Base color"));
        if (!picked.isValid())
            return;
        seedColor = picked;
        seedSwatch->setStyleSheet(swatchStyle(seedColor));
    });

    connect(genBtn, &QPushButton::clicked, this, [this, schemeCombo, modeCombo]() {
        generateInto(seedColor, schemeCombo->currentIndex(), modeCombo->currentIndex() == 0);
    });

    connect(randBtn, &QPushButton::clicked, this, [this, schemeCombo, modeCombo]() {
        auto *rng = QRandomGenerator::global();
        const qreal h = rng->generateDouble() * 360.0;
        const qreal s = 0.55 + rng->generateDouble() * 0.40; // 0.55..0.95
        const qreal l = 0.45 + rng->generateDouble() * 0.20; // 0.45..0.65
        seedColor = QColor::fromHslF(static_cast<float>(h / 360.0), static_cast<float>(s), static_cast<float>(l));
        seedSwatch->setStyleSheet(swatchStyle(seedColor));
        const int scheme = static_cast<int>(rng->generateDouble() * Core::Theme::schemeCount);
        schemeCombo->setCurrentIndex(scheme);
        generateInto(seedColor, scheme, modeCombo->currentIndex() == 0);
    });

    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    auto *content = new QWidget(scroll);
    auto *layout = new QVBoxLayout(content);

    QString currentGroup;
    for (const TokenDescriptor &d : Core::Theme::registry()) {
        const QString group = QString::fromUtf8(d.group);
        if (group != currentGroup) {
            currentGroup = group;
            if (layout->count() > 0)
                layout->addSpacing(8);
            auto *header = new QLabel(group, content);
            QFont f = header->font();
            f.setBold(true);
            header->setFont(f);
            layout->addWidget(header);
        }

        auto *row = new QHBoxLayout();
        row->addWidget(new QLabel(QString::fromUtf8(d.label), content));
        row->addStretch(1);

        auto *swatch = new QPushButton(content);
        swatch->setFixedSize(48, 22);
        swatch->setCursor(Qt::PointingHandCursor);
        swatch->setStyleSheet(swatchStyle(Manager::instance().color(d.token)));
        row->addWidget(swatch);

        auto *reset = new QToolButton(content);
        reset->setText(tr("Reset"));
        row->addWidget(reset);

        layout->addLayout(row);

        const Token token = d.token;
        const bool supportsAlpha = d.supportsAlpha;
        const QString title = QString::fromUtf8(d.label);
        swatches.insert(token, swatch);

        connect(swatch, &QPushButton::clicked, this,
                [this, token, supportsAlpha, title, swatch]() {
                    QColorDialog::ColorDialogOptions opts;
                    if (supportsAlpha)
                        opts |= QColorDialog::ShowAlphaChannel;
                    const QColor picked = QColorDialog::getColor(Manager::instance().color(token), this, title, opts);
                    if (!picked.isValid())
                        return;
                    Manager::instance().setOverride(token, picked);
                    Manager::instance().apply();
                    Manager::instance().save();
                    swatch->setStyleSheet(swatchStyle(picked));
                });

        connect(reset, &QToolButton::clicked, this, [this, token, swatch]() {
            Manager::instance().clearOverride(token);
            Manager::instance().apply();
            Manager::instance().save();
            swatch->setStyleSheet(swatchStyle(Manager::instance().color(token)));
        });
    }

    layout->addStretch(1);
    scroll->setWidget(content);
    outer->addWidget(scroll, 1);

    auto *actions = new QHBoxLayout();
    auto *resetAll = new QPushButton(tr("Reset all"), this);
    auto *exportBtn = new QPushButton(tr("Export"), this);
    auto *importBtn = new QPushButton(tr("Import"), this);
    actions->addWidget(resetAll);
    actions->addStretch(1);
    actions->addWidget(exportBtn);
    actions->addWidget(importBtn);
    outer->addLayout(actions);

    connect(resetAll, &QPushButton::clicked, this, [this]() {
        Manager::instance().resetAll();
        Manager::instance().apply();
        Manager::instance().save();
        rebuildSwatches();
    });

    connect(exportBtn, &QPushButton::clicked, this, [this]() {
        const QString path = QFileDialog::getSaveFileName(
                this, tr("Export theme"), "theme.json",
                tr("Theme files (*.json)"));
        if (!path.isEmpty())
            Manager::instance().exportTo(path);
    });

    connect(importBtn, &QPushButton::clicked, this, [this]() {
        const QString path = QFileDialog::getOpenFileName(this, tr("Import theme"), QString(), tr("Theme files (*.json)"));
        if (path.isEmpty())
            return;
        if (Manager::instance().importFrom(path)) {
            Manager::instance().apply();
            Manager::instance().save();
            rebuildSwatches();
        }
    });
}

void AppearancePage::rebuildSwatches()
{
    for (auto it = swatches.begin(); it != swatches.end(); ++it) {
        const Token token = it.key();
        it.value()->setStyleSheet(swatchStyle(Manager::instance().color(token)));
    }
}

void AppearancePage::generateInto(const QColor &seed, int schemeIndex, bool dark)
{
    const auto scheme = static_cast<Core::Theme::Scheme>(std::clamp(schemeIndex, 0, Core::Theme::schemeCount - 1));
    Manager::instance().setOverrides(Core::Theme::generate(seed, scheme, dark));
    Manager::instance().apply();
    Manager::instance().save();
    rebuildSwatches();
}

} // namespace UI
} // namespace Acheron
