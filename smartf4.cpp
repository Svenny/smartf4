#include "smartf4.hpp"

#include <coreplugin/icore.h>
#include <coreplugin/icontext.h>
#include <coreplugin/idocument.h>
#include <coreplugin/actionmanager/actionmanager.h>
#include <coreplugin/actionmanager/command.h>
#include <coreplugin/editormanager/editormanager.h>

#include <cppeditor/cppeditorconstants.h>
#include <cppeditor/cppmodelmanager.h>
#include <cppeditor/cppprojectfile.h>
#include <cppeditor/cpptoolsreuse.h>
#include <cppeditor/projectinfo.h>

#include <projectexplorer/project.h>
#include <projectexplorer/projecttree.h>

#include <utils/fileutils.h>
#include <utils/qtcassert.h>
#include <utils/mimetypes/mimedatabase.h>

#include <QAction>
#include <QSet>

namespace SmartF4::Internal
{

constexpr static char SWITCH_ACTION_ID[] = "SmartF4.SwitchHeaderSource";
constexpr static char RESET_ACTION_ID[] = "SmartF4.ResetSwitchCache";

bool SmartF4Plugin::initialize(const QStringList &arguments, QString *errorString)
{
	Q_UNUSED(arguments)
	Q_UNUSED(errorString)

	Core::Context context(CppEditor::Constants::CPPEDITOR_ID);

	auto *switch_action = new QAction(tr("Switch header/source"), this);
	auto *switch_cmd = Core::ActionManager::registerAction(switch_action, SWITCH_ACTION_ID, context);
	switch_cmd->setDefaultKeySequence(QKeySequence(Qt::Key_F4));
	connect(switch_action, &QAction::triggered, this, &SmartF4Plugin::switchHeaderSource);

	auto *reset_action = new QAction(tr("Reset switch cache"), this);
	auto *reset_cmd = Core::ActionManager::registerAction(reset_action, RESET_ACTION_ID, context);
	reset_cmd->setDefaultKeySequence(QKeySequence("Ctrl+Shift+F4"));
	connect(reset_action, &QAction::triggered, this, &SmartF4Plugin::resetSwitchCache);

	return true;
}

// Returns the set of filenames which can match a given file
static QSet<QString> matchingFilenames(const QFileInfo &fileInfo)
{
	static const QStringList headerSuffixes = [](){
		QStringList list;
		list += Utils::mimeTypeForName(CppEditor::Constants::C_SOURCE_MIMETYPE).suffixes();
		list += Utils::mimeTypeForName(CppEditor::Constants::CPP_SOURCE_MIMETYPE).suffixes();
		list += Utils::mimeTypeForName(CppEditor::Constants::OBJECTIVE_C_SOURCE_MIMETYPE).suffixes();
		list += Utils::mimeTypeForName(CppEditor::Constants::OBJECTIVE_CPP_SOURCE_MIMETYPE).suffixes();
		list += Utils::mimeTypeForName(CppEditor::Constants::CUDA_SOURCE_MIMETYPE).suffixes();
		return list;
	}();

	static const QStringList sourceSuffixes = [](){
		QStringList list;
		list += Utils::mimeTypeForName(CppEditor::Constants::C_HEADER_MIMETYPE).suffixes();
		list += Utils::mimeTypeForName(CppEditor::Constants::CPP_HEADER_MIMETYPE).suffixes();
		return list;
	}();

	const CppEditor::ProjectFile::Kind kind = CppEditor::ProjectFile::classify(fileInfo.fileName());
	const bool isHeader = CppEditor::ProjectFile::isHeader(kind);
	const bool isSource = CppEditor::ProjectFile::isSource(kind);
	if (!isHeader && !isSource) {
		return {};
	}

	const QStringList &suffixes = isHeader ? headerSuffixes : sourceSuffixes;
	const QString baseName = fileInfo.completeBaseName();

	QSet<QString> result;
	for (const QString &suffix : suffixes) {
		result.insert(baseName + '.' + suffix);
	}
	return result;
}

static QString findAnyNameNearby(const QFileInfo &fileInfo, const QSet<QString> &candidateNames)
{
	const QDir dir = fileInfo.absoluteDir();

	for (const QString &name : candidateNames) {
		QString path = dir.absoluteFilePath(name);
		if (QFileInfo(path).isFile()) {
			return path;
		}
	}

	return {};
}

static QStringList findFilesInProject(const ProjectExplorer::Project &project, const QSet<QString> &names)
{
	QStringList found;

	const Utils::FilePaths paths = project.files(ProjectExplorer::Project::AllFiles);
	for (const auto &path : paths) {
		const QFileInfo info = path.toFileInfo();
		if (names.contains(info.fileName())) {
			found += info.absoluteFilePath();
		}
	}

	return found;
}

static int stringDistance(QStringView a, QStringView b)
{
	// Eliminate common prefix
	while (!a.empty() && !b.empty() && a[0] == b[0]) {
		a = a.mid(1);
		b = b.mid(1);
	}

	// Eliminate common suffix
	while (!a.empty() && !b.empty() && a.back() == b.back()) {
		a.chop(1);
		b.chop(1);
	}

	// As trivial as it can be - consider all remaining characters as difference
	// TODO: replace with more advanced heuristic (Levenstein distance?)
	return a.length() + b.length();
}

static QString findNearestInProject(const ProjectExplorer::Project &project, const QString &referenceDir,
                                    const QSet<QString> &candidateNames)
{
	QString bestPath;
	int bestDistance = INT_MAX;

	const QStringList foundFiles = findFilesInProject(project, candidateNames);
	for (const auto &file : foundFiles) {
		const QFileInfo info(file);
		if (!info.isFile()) {
			continue;
		}

		// Find the distance between full directory paths, assume
		// that "true" candidate will have the smallest distance
		const QString dir = info.canonicalPath();
		const int distance = stringDistance(dir, referenceDir);

		if (distance < bestDistance) {
			bestPath = file;
			bestDistance = distance;
		}
	}

	return bestPath;
}

QString SmartF4Plugin::nearestHeaderOrSource(const QString &fileName)
{
	const QFileInfo fileInfo(fileName);
	const QString absolutePath = fileInfo.canonicalFilePath();

	if (auto iter = m_switchCache.find(absolutePath); iter != m_switchCache.end()) {
		return iter.value();
	}

	QSet<QString> candidateFileNames = matchingFilenames(fileInfo);
	if (candidateFileNames.empty()) {
		return {};
	}

	// Find files in just the same directory
	QString neighbour = findAnyNameNearby(fileInfo, candidateFileNames);
	if (!neighbour.isEmpty()) {
		m_switchCache[absolutePath] = neighbour;
		m_switchCache[neighbour] = absolutePath;
		return neighbour;
	}

	const QString absoluteDir = fileInfo.canonicalPath();

	const ProjectExplorer::Project *currentProject = ProjectExplorer::ProjectTree::currentProject();
	if (currentProject) {
		// Find files in the current project
		QString path = findNearestInProject(*currentProject, absoluteDir, candidateFileNames);
		if (!path.isEmpty()) {
			m_switchCache[absolutePath] = path;
			m_switchCache[path] = absolutePath;
			return path;
		}
	}

	// Find files in other projects
	const auto *modelManager = CppEditor::CppModelManager::instance();
	const auto projectInfos = modelManager->projectInfos();

	for (const auto &info : projectInfos) {
		const ProjectExplorer::Project *project = CppEditor::projectForProjectInfo(*info);
		if (!project || project == currentProject) {
			// We have already checked the current project
			 continue;
		}

		QString path = findNearestInProject(*project, absoluteDir, candidateFileNames);
		if (!path.isEmpty()) {
			m_switchCache[absolutePath] = path;
			m_switchCache[path] = absolutePath;
			return path;
		}
	}

	// Cache the unsuccessful result as empty string
	m_switchCache[absolutePath].clear();
	return {};
}

void SmartF4Plugin::switchHeaderSource()
{
	const Core::IDocument *currentDocument = Core::EditorManager::currentDocument();
	QTC_ASSERT(currentDocument, return);

	const QString otherFile = nearestHeaderOrSource(currentDocument->filePath().toString());
	if (!otherFile.isEmpty()) {
		Core::EditorManager::openEditor(otherFile);
	}
}

void SmartF4Plugin::resetSwitchCache()
{
	m_switchCache.clear();
}

} // namespace SmartF4
