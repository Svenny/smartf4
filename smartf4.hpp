#pragma once

#include <extensionsystem/iplugin.h>

#include <QHash>

namespace SmartF4::Internal
{

class SmartF4Plugin final : public ExtensionSystem::IPlugin {
	Q_OBJECT
	Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QtCreatorPlugin" FILE "SmartF4.json")

public:
	SmartF4Plugin() = default;
	SmartF4Plugin(SmartF4Plugin &&) = delete;
	SmartF4Plugin(const SmartF4Plugin &) = delete;
	SmartF4Plugin &operator = (SmartF4Plugin &&) = delete;
	SmartF4Plugin &operator = (const SmartF4Plugin &) = delete;
	~SmartF4Plugin() override = default;

	bool initialize(const QStringList &arguments, QString *errorString) override;

private:
	QString nearestHeaderOrSource(const QString &file_name);
	void switchHeaderSource();
	void resetSwitchCache();

	QHash<QString, QString> m_switchCache;
};

} // namespace SmartF4::Internal

