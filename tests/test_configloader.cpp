#include "config/ConfigLoader.h"

#include <QTest>

class TestConfigLoader : public QObject
{
    Q_OBJECT

private slots:
    void validConfig();
    void invalidSyntax();
    void missingRequiredFields();
    void duplicateIds();
    void duplicateShortcuts();
    void shortcutOutOfRange();
    void invalidColumns();
    void invalidOverlayPosition();
    void emptyCameraList();
    void subUrlOptional();
    void nameOptional();
};

void TestConfigLoader::validConfig()
{
    const QString yaml = R"(
layout:
  columns: 4

overlay:
  enabled: true
  position: bottom
  showName: true
  showStatus: true

cameras:
  - id: front
    name: "Bejarat"
    shortcut: 1
    mainUrl: "rtsp://192.168.1.101:554/main"
    subUrl: "rtsp://192.168.1.101:554/sub"

  - id: garden
    name: "Kert"
    shortcut: 2
    mainUrl: "rtsp://192.168.1.102:554/main"
)";
    const auto result = ConfigLoader::loadFromString(yaml);
    QVERIFY(result.ok);
    QVERIFY(result.errors.isEmpty());
    QCOMPARE(result.config.layout.columns, 4);
    QCOMPARE(result.config.cameras.size(), 2);
    QCOMPARE(result.config.cameras[0].id, QStringLiteral("front"));
    QCOMPARE(result.config.cameras[0].shortcut, 1);
    QVERIFY(result.config.cameras[0].hasSub());
    QVERIFY(!result.config.cameras[1].hasSub());
}

void TestConfigLoader::invalidSyntax()
{
    const auto result = ConfigLoader::loadFromString(QStringLiteral("cameras: [this is not: valid: yaml"));
    QVERIFY(!result.ok);
    QVERIFY(!result.errors.isEmpty());
}

void TestConfigLoader::missingRequiredFields()
{
    const QString yaml = R"(
cameras:
  - name: "No id or mainUrl"
)";
    const auto result = ConfigLoader::loadFromString(yaml);
    QVERIFY(!result.ok);
    QVERIFY(result.errors.size() >= 2);
}

void TestConfigLoader::duplicateIds()
{
    const QString yaml = R"(
cameras:
  - id: front
    mainUrl: "rtsp://a/main"
  - id: front
    mainUrl: "rtsp://b/main"
)";
    const auto result = ConfigLoader::loadFromString(yaml);
    QVERIFY(!result.ok);
    QVERIFY(result.errors.join('\n').contains(QStringLiteral("duplicate camera id")));
}

void TestConfigLoader::duplicateShortcuts()
{
    const QString yaml = R"(
cameras:
  - id: front
    shortcut: 1
    mainUrl: "rtsp://a/main"
  - id: back
    shortcut: 1
    mainUrl: "rtsp://b/main"
)";
    const auto result = ConfigLoader::loadFromString(yaml);
    QVERIFY(!result.ok);
    QVERIFY(result.errors.join('\n').contains(QStringLiteral("duplicate shortcut")));
}

void TestConfigLoader::shortcutOutOfRange()
{
    const QString yaml = R"(
cameras:
  - id: front
    shortcut: 10
    mainUrl: "rtsp://a/main"
)";
    const auto result = ConfigLoader::loadFromString(yaml);
    QVERIFY(!result.ok);
    QVERIFY(result.errors.join('\n').contains(QStringLiteral("0-9")));
}

void TestConfigLoader::invalidColumns()
{
    const QString yaml = R"(
layout:
  columns: 0
cameras:
  - id: front
    mainUrl: "rtsp://a/main"
)";
    const auto result = ConfigLoader::loadFromString(yaml);
    QVERIFY(!result.ok);
    QVERIFY(result.errors.join('\n').contains(QStringLiteral("layout.columns")));
}

void TestConfigLoader::invalidOverlayPosition()
{
    const QString yaml = R"(
overlay:
  position: middle
cameras:
  - id: front
    mainUrl: "rtsp://a/main"
)";
    const auto result = ConfigLoader::loadFromString(yaml);
    QVERIFY(!result.ok);
    QVERIFY(result.errors.join('\n').contains(QStringLiteral("overlay.position")));
}

void TestConfigLoader::emptyCameraList()
{
    const auto result = ConfigLoader::loadFromString(QStringLiteral("layout:\n  columns: 4\n"));
    QVERIFY(!result.ok);
    QVERIFY(result.errors.join('\n').contains(QStringLiteral("cameras")));
}

void TestConfigLoader::subUrlOptional()
{
    const QString yaml = R"(
cameras:
  - id: front
    mainUrl: "rtsp://a/main"
)";
    const auto result = ConfigLoader::loadFromString(yaml);
    QVERIFY(result.ok);
    QVERIFY(!result.config.cameras[0].hasSub());
}

void TestConfigLoader::nameOptional()
{
    const QString yaml = R"(
cameras:
  - id: front
    mainUrl: "rtsp://a/main"
)";
    const auto result = ConfigLoader::loadFromString(yaml);
    QVERIFY(result.ok);
    QVERIFY(!result.config.cameras[0].hasName());
}

QTEST_APPLESS_MAIN(TestConfigLoader)
#include "test_configloader.moc"
