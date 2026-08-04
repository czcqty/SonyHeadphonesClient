#include <QCoreApplication>
#include <QColor>
#include <QImage>
#include <QRect>
#include <QStringList>

#include <algorithm>
#include <cmath>
#include <cstdio>

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    const QStringList args = app.arguments();
    if (args.size() < 4) {
        std::fprintf(stderr,
                     "usage: visual_diff <reference.png> <actual.png> <diff.png> "
                     "[--ignore x,y,w,h] [--threshold n] [--max-ratio r]\n");
        return 2;
    }

    QImage reference(args.at(1));
    QImage actual(args.at(2));
    if (reference.isNull() || actual.isNull() || reference.size() != actual.size()) {
        std::fprintf(stderr, "images are unreadable or have different dimensions\n");
        return 2;
    }

    QVector<QRect> masks;
    int channelThreshold = 8;
    double maxRatio = 0.0025;
    for (int index = 4; index < args.size(); ++index) {
        if (args.at(index) == QStringLiteral("--ignore") && index + 1 < args.size()) {
            const QStringList values = args.at(++index).split(QLatin1Char(','));
            if (values.size() == 4)
                masks.push_back(QRect(values.at(0).toInt(), values.at(1).toInt(),
                                      values.at(2).toInt(), values.at(3).toInt()));
        } else if (args.at(index) == QStringLiteral("--threshold") && index + 1 < args.size()) {
            channelThreshold = std::clamp(args.at(++index).toInt(), 0, 255);
        } else if (args.at(index) == QStringLiteral("--max-ratio") && index + 1 < args.size()) {
            maxRatio = std::clamp(args.at(++index).toDouble(), 0.0, 1.0);
        }
    }

    reference = reference.convertToFormat(QImage::Format_RGBA8888);
    actual = actual.convertToFormat(QImage::Format_RGBA8888);
    QImage diff(reference.size(), QImage::Format_RGBA8888);
    diff.fill(Qt::transparent);

    qsizetype compared = 0;
    qsizetype mismatched = 0;
    for (int y = 0; y < reference.height(); ++y) {
        for (int x = 0; x < reference.width(); ++x) {
            const QPoint point(x, y);
            if (std::any_of(masks.cbegin(), masks.cend(),
                            [&point](const QRect& mask) { return mask.contains(point); }))
                continue;

            ++compared;
            const QColor expected = reference.pixelColor(x, y);
            const QColor observed = actual.pixelColor(x, y);
            const int delta = std::max({
                std::abs(expected.red() - observed.red()),
                std::abs(expected.green() - observed.green()),
                std::abs(expected.blue() - observed.blue()),
                std::abs(expected.alpha() - observed.alpha()),
            });
            if (delta > channelThreshold) {
                ++mismatched;
                diff.setPixelColor(x, y, QColor(255, 0, 0, std::clamp(delta * 2, 64, 255)));
            }
        }
    }

    if (!diff.save(args.at(3))) {
        std::fprintf(stderr, "failed to save diff image\n");
        return 2;
    }

    const double ratio = compared == 0 ? 0.0
        : static_cast<double>(mismatched) / static_cast<double>(compared);
    std::printf("pixels=%lld mismatched=%lld ratio=%.8f max=%.8f\n",
                static_cast<long long>(compared),
                static_cast<long long>(mismatched),
                ratio,
                maxRatio);
    return ratio <= maxRatio ? 0 : 1;
}
