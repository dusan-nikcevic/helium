#include "waveform.h"
#include <QSGGeometryNode>
#include <QSGFlatColorMaterial>
#include <cmath>
#include <cstdint>
Waveform::Waveform(QQuickItem *parent) : QQuickItem(parent) { setFlag(ItemHasContents); }
void Waveform::setSeed(int seed) {
    if (seed == m_seed) return;
    m_seed = seed;
    emit seedChanged();
    update();
}
void Waveform::setColor(const QColor &color) {
    if (color == m_color) return;
    m_color = color;
    emit colorChanged();
    update();
}
void Waveform::geometryChange(const QRectF &next, const QRectF &previous) {
    QQuickItem::geometryChange(next, previous);
    if (next.size() != previous.size()) update();
}
QSGNode *Waveform::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *) {
    if (width() < 1 || height() < 1 || !std::isfinite(width()) || !std::isfinite(height())) {
        delete oldNode;
        return nullptr;
    }
    auto *node = static_cast<QSGGeometryNode *>(oldNode);
    const int samples = qMax(16, qRound(width() / 2.2));
    if (!node) {
        node = new QSGGeometryNode;
        auto *geometry = new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(), samples * 2);
        geometry->setDrawingMode(QSGGeometry::DrawTriangleStrip);
        node->setGeometry(geometry);
        node->setFlag(QSGNode::OwnsGeometry);
        node->setMaterial(new QSGFlatColorMaterial);
        node->setFlag(QSGNode::OwnsMaterial);
    }
    auto *geometry = node->geometry();
    if (geometry->vertexCount() != samples * 2) geometry->allocate(samples * 2);
    auto *vertices = geometry->vertexDataAsPoint2D();
    uint32_t t = uint32_t(m_seed);
    const double mid = height() / 2;
    for (int i = 0; i < samples; ++i) {
        t += 0x6D2B79F5u;
        uint32_t r = (t ^ (t >> 15)) * (1u | t);
        r ^= r + ((r ^ (r >> 7)) * (61u | r));
        const double random = double(r ^ (r >> 14)) / 4294967296.0;
        const double a = 0.2 + 0.8 * random;
        const double env = std::sqrt(std::sin(3.141592653589793 * i / (samples - 1)));
        const double amplitude = (0.08 + a * a * 0.92) * mid * env;
        const float x = float(std::round(double(i) / (samples - 1) * width() * 10) / 10);
        vertices[2 * i].set(x, float(std::round((mid - amplitude) * 10) / 10));
        vertices[2 * i + 1].set(x, float(std::round((mid + amplitude) * 10) / 10));
    }
    static_cast<QSGFlatColorMaterial *>(node->material())->setColor(m_color);
    node->markDirty(QSGNode::DirtyGeometry | QSGNode::DirtyMaterial);
    return node;
}
