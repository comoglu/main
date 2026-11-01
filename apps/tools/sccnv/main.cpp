/***************************************************************************
 * Copyright (C) GFZ Potsdam                                               *
 * All rights reserved.                                                    *
 *                                                                         *
 * GNU Affero General Public License Usage                                 *
 * This file may be used under the terms of the GNU Affero                 *
 * Public License version 3.0 as published by the Free Software Foundation *
 * and appearing in the file LICENSE included in the packaging of this     *
 * file. Please review the following information to ensure the GNU Affero  *
 * Public License version 3.0 requirements will be met:                    *
 * https://www.gnu.org/licenses/agpl-3.0.html.                             *
 ***************************************************************************/

#define SEISCOMP_COMPONENT sccnv

#include <seiscomp/logging/log.h>
#include <seiscomp/client/application.h>
#include <seiscomp/core/strings.h>
#include <seiscomp/datamodel/version.h>
#include <seiscomp/io/importer.h>
#include <seiscomp/io/exporter.h>
#include <seiscomp/io/archive/xmlarchive.h>
#include <seiscomp/system/environment.h>

#include <cerrno>
#include <cstring>
#include <fstream>
#include <iostream>
#include <unistd.h>


using namespace Seiscomp;
using namespace Seiscomp::Client;
using namespace Seiscomp::Core;
using namespace Seiscomp::IO;


namespace {


const char *DefaultFormat = "scml";


template <typename FACTORY>
std::string serviceList() {
	std::string list;
	typename FACTORY::ServiceNames *names = FACTORY::Services();
	if ( names ) {
		for ( const auto &name : *names ) {
			if ( !list.empty() ) {
				list += ", ";
			}
			list += name;
		}
		delete names;
	}
	return list;
}


/**
 * @brief Splits a stream specification [format:][file] into format and file.
 *
 * The part in front of the first colon is only taken as format if it names a
 * registered format. Otherwise the whole specification is a file name if it
 * is clearly a path (contains a slash in front of the colon) or, for input,
 * if a file with that name exists. This allows file names with colons, e.g.
 * ISO time stamps.
 * @return false if the prefix is neither a known format nor a file name
 */
template <typename FACTORY>
bool splitStream(const std::string &spec, bool input,
                 std::string &format, std::string &file) {
	size_t pos = spec.find(':');
	format.clear();
	file = spec;

	if ( pos == std::string::npos ) {
		return true;
	}

	std::string prefix = spec.substr(0, pos);
	if ( prefix.empty() || FACTORY::Find(prefix) ) {
		format = prefix;
		file = spec.substr(pos + 1);
		return true;
	}

	if ( prefix.find('/') != std::string::npos ) {
		return true;
	}

	if ( input && !access(spec.c_str(), F_OK) ) {
		return true;
	}

	SEISCOMP_ERROR("Unknown %s format '%s', available formats: %s",
	               input ? "input" : "output", prefix.c_str(),
	               serviceList<FACTORY>().c_str());
	SEISCOMP_ERROR("To use a file name containing ':' either state the "
	               "format, e.g. %s:%s, or the path, e.g. ./%s",
	               DefaultFormat, spec.c_str(), spec.c_str());
	return false;
}


/**
 * @brief Guesses the format of a file from its first bytes.
 * @return The detected format or an empty string if unknown. "quakeml" is
 *         returned for QuakeML which has no importer.
 */
std::string detectFormat(std::istream &is) {
	char data[4096];
	is.read(data, sizeof(data));
	std::string head(data, is.gcount());

	// Binary archive header, version 1 (SCBA) or 2 (SCBB)
	if ( head.size() >= 4 && !head.compare(0, 3, "SCB") &&
	     (head[3] == 'A' || head[3] == 'B') ) {
		return "vbinary";
	}

	size_t pos = 0;
	// Skip UTF-8 byte order mark
	if ( !head.compare(0, 3, "\xEF\xBB\xBF") ) {
		pos = 3;
	}

	pos = head.find_first_not_of(" \t\r\n", pos);
	if ( pos == std::string::npos ) {
		return std::string();
	}

	if ( head[pos] == '{' ) {
		return "json";
	}

	// Find the name of the XML root element, skip the declaration,
	// processing instructions, comments and doctype
	while ( pos != std::string::npos && head[pos] == '<' ) {
		if ( !head.compare(pos, 4, "<!--") ) {
			pos = head.find("-->", pos);
			if ( pos != std::string::npos ) {
				pos += 3;
			}
		}
		else if ( pos + 1 < head.size() && (head[pos+1] == '?' || head[pos+1] == '!') ) {
			pos = head.find('>', pos);
			if ( pos != std::string::npos ) {
				pos += 1;
			}
		}
		else {
			size_t end = head.find_first_of(" \t\r\n/>", pos + 1);
			if ( end == std::string::npos ) {
				break;
			}

			std::string name = head.substr(pos + 1, end - pos - 1);
			// Strip the namespace prefix
			size_t colon = name.find(':');
			if ( colon != std::string::npos ) {
				name.erase(0, colon + 1);
			}

			if ( name == "seiscomp" ) {
				return "scml";
			}
			if ( name == "inventory" ) {
				return "arclink";
			}
			if ( name == "quakeml" ) {
				return "quakeml";
			}
			if ( name == "FDSNStationXML" ) {
				return "fdsnxml";
			}
			break;
		}

		if ( pos != std::string::npos ) {
			pos = head.find_first_not_of(" \t\r\n", pos);
		}
	}

	return std::string();
}


}


class ConvertApp : public Application {
	public:
		ConvertApp(int argc, char **argv) : Application(argc, argv) {
			setMessagingEnabled(false);
			setDatabaseEnabled(false, false);
			setLoggingToStdErr(true);
			_indentation = 2;
		}

	protected:
		void createCommandLineDescription() {
			commandline().addGroup("Formats");
			commandline().addOption("Formats", "format-list",
			                        "List all supported formats.");
			commandline().addOption("Formats", "list-formats",
			                        "Same as --format-list.");
			commandline().addGroup("Input");
			commandline().addOption("Input", "input,i",
			                        "Input stream [format:][file]. Default: scml:-. "
			                        "Without format, the format of input files is "
			                        "detected from the content, falling back to scml.",
			                        &_inputStream, false);
			commandline().addOption("Input", "strict",
			                        "Exit with an error if the input is read with "
			                        "errors instead of converting what was read.");
			commandline().addGroup("Output");
			commandline().addOption("Output", "output,o",
			                        "Output stream [format:][file]. Default: scml:-",
			                        &_outputStream, false);
			commandline().addOption("Output", "formatted,f",
			                        "Use formatted output.");
			commandline().addOption("Output", "indent", "Formatted line indent.",
			                        &_indentation);
		}

		void printUsage() const {
			std::cout << "Usage:" << std::endl << "  sccnv [options]"
			          << std::endl << std::endl
			          << "Convert content between different formats."
			          << std::endl;

			Client::Application::printUsage();

			std::cout << "Examples:" << std::endl
			          << "Print the list of supported formats"
			          << std::endl
			          << "  sccnv --format-list"
			          << std::endl << std::endl
			          << "Convert an event parameter file in SCML format to QuakeML, store the content in a file"
			          << std::endl
			          << "  sccnv -i seiscomp.xml -o qml1.2:quakeml.xml"
			          << std::endl << std::endl
			          << "Convert a file in JSON format, detected automatically, to formatted SCML on stdout"
			          << std::endl
			          << "  sccnv -i event.json -f"
			          << std::endl << std::endl;
		}

		bool run() {
			if (( commandline().hasOption("format-list") ) || ( commandline().hasOption("list-formats") )) {
				std::cout << "Input formats: " << serviceList<ImporterFactory>() << std::endl;
				std::cout << "Output formats: " << serviceList<ExporterFactory>() << std::endl;
				return true;
			}

			// Reading from a terminal is most probably not intended and would
			// block. Explicitly given input (-i -) is read anyway.
			if ( _inputStream.empty() && isatty(STDIN_FILENO) ) {
				SEISCOMP_ERROR("No input given: use --input or pipe data to stdin");
				return false;
			}

			std::string inputFormat, inputFile;
			std::string outputFormat, outputFile;

			if ( !splitStream<ImporterFactory>(_inputStream, true, inputFormat, inputFile) ) {
				return false;
			}

			if ( !splitStream<ExporterFactory>(_outputStream, false, outputFormat, outputFile) ) {
				return false;
			}

			if ( inputFile.empty() ) {
				inputFile = "-";
			}
			if ( outputFile.empty() ) {
				outputFile = "-";
			}

			ExporterPtr exp = ExporterFactory::Create(outputFormat.empty() ? DefaultFormat : outputFormat.c_str());
			if ( !exp ) {
				SEISCOMP_ERROR("Unknown output format %s", outputFormat.c_str());
				return false;
			}

			std::ifstream ifs;
			if ( inputFile != "-" ) {
				ifs.open(inputFile.c_str(), std::ios_base::in | std::ios_base::binary);
				if ( !ifs.is_open() ) {
					SEISCOMP_ERROR("Could not open input file %s: %s",
					               inputFile.c_str(), strerror(errno));
					return false;
				}

				if ( inputFormat.empty() ) {
					inputFormat = detectFormat(ifs);
					ifs.clear();
					ifs.seekg(0);

					if ( inputFormat == "quakeml" ) {
						std::string version = Core::toString(int(DataModel::Version::Major)) + "." +
						                      Core::toString(int(DataModel::Version::Minor));
						std::string xsl = Environment::Instance()->shareDir() + "/xml/" +
						                  version + "/quakeml_1.2__scml_" + version + ".xsl";
						SEISCOMP_ERROR("%s is QuakeML which cannot be read by sccnv", inputFile.c_str());
						SEISCOMP_ERROR("Convert it to SCML with an XSLT processor, e.g.:");
						SEISCOMP_ERROR("  xsltproc %s %s > %s.scml",
						               xsl.c_str(), inputFile.c_str(), inputFile.c_str());
						SEISCOMP_ERROR("Long QuakeML IDs can be shortened by removing their prefix, e.g.:");
						SEISCOMP_ERROR("  xsltproc --stringparam ID_PREFIX quakeml:us.anss.org/ %s %s > %s.scml",
						               xsl.c_str(), inputFile.c_str(), inputFile.c_str());
						SEISCOMP_ERROR("Recommended parameters per agency are listed in the header of %s",
						               xsl.c_str());
						return false;
					}

					if ( !inputFormat.empty() && !ImporterFactory::Find(inputFormat) ) {
						SEISCOMP_ERROR("%s is in %s format which requires a plugin, "
						               "e.g. --plugins %s",
						               inputFile.c_str(), inputFormat.c_str(),
						               inputFormat.c_str());
						return false;
					}

					if ( inputFormat.empty() ) {
						SEISCOMP_DEBUG("Could not detect the input format, assuming %s",
						               DefaultFormat);
					}
					else {
						SEISCOMP_DEBUG("Detected input format %s", inputFormat.c_str());
					}
				}
			}

			if ( inputFormat.empty() ) {
				inputFormat = DefaultFormat;
			}

			ImporterPtr imp = ImporterFactory::Create(inputFormat.c_str());
			if ( !imp ) {
				SEISCOMP_ERROR("Unknown input format %s", inputFormat.c_str());
				return false;
			}

			BaseObjectPtr obj = imp->read(ifs.is_open() ? ifs.rdbuf() : std::cin.rdbuf());
			if ( !imp->withoutErrors() ) {
				if ( commandline().hasOption("strict") ) {
					SEISCOMP_ERROR("Got errors during reading");
					return false;
				}

				SEISCOMP_WARNING("Got errors during reading");
			}

			if ( !obj ) {
				SEISCOMP_ERROR("Input empty, nothing to do");
				return false;
			}

			exp->setFormattedOutput(commandline().hasOption("formatted"));
			exp->setIndent(_indentation);
			if ( !exp->write(outputFile.c_str(), obj.get()) ) {
				SEISCOMP_ERROR("Error while writing to %s", outputFile.c_str());
				return false;
			}

			return true;
		}

	private:
		std::string _inputStream;
		std::string _outputStream;
		int _indentation;
};


int main(int argc, char **argv) {
	int retCode = EXIT_SUCCESS;

	// Create an own block to make sure the application object
	// is destroyed when printing the overall objectcount
	{
		ConvertApp app(argc, argv);
		retCode = app.exec();
	}

	return retCode;
}
