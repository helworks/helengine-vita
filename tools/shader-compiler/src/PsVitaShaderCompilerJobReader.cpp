#include "PsVitaShaderCompilerJobReader.hpp"

#include <cctype>
#include <fstream>
#include <set>
#include <sstream>

namespace {
    /// Parses the limited JSON value forms used by the fixed shader compiler manifest schema.
    class JsonCursor final {
    public:
        /// Initializes a cursor over a complete UTF-8 JSON manifest.
        explicit JsonCursor(const std::string& text)
            : Text(text), Position(0u) {
        }

        /// Consumes one expected punctuation character after skipping JSON whitespace.
        bool Consume(char expected) {
            SkipWhitespace();
            if (Position >= Text.size() || Text[Position] != expected) {
                return false;
            }

            ++Position;
            return true;
        }

        /// Reads one required JSON string, including common escaped characters.
        bool ReadString(std::string& value) {
            if (!Consume('"')) {
                return false;
            }

            value.clear();
            while (Position < Text.size()) {
                char character = Text[Position++];
                if (character == '"') {
                    return true;
                }
                if (static_cast<unsigned char>(character) < 0x20u) {
                    return false;
                }
                if (character != '\\') {
                    value.push_back(character);
                    continue;
                }
                if (Position >= Text.size()) {
                    return false;
                }
                char escaped = Text[Position++];
                if (escaped == '"' || escaped == '\\' || escaped == '/') {
                    value.push_back(escaped);
                } else if (escaped == 'b') {
                    value.push_back('\b');
                } else if (escaped == 'f') {
                    value.push_back('\f');
                } else if (escaped == 'n') {
                    value.push_back('\n');
                } else if (escaped == 'r') {
                    value.push_back('\r');
                } else if (escaped == 't') {
                    value.push_back('\t');
                } else {
                    return false;
                }
            }

            return false;
        }

        /// Reads one unsigned decimal JSON number.
        bool ReadUnsigned(std::size_t& value) {
            SkipWhitespace();
            if (Position >= Text.size() || !std::isdigit(static_cast<unsigned char>(Text[Position]))) {
                return false;
            }

            value = 0u;
            while (Position < Text.size() && std::isdigit(static_cast<unsigned char>(Text[Position]))) {
                value = value * 10u + static_cast<std::size_t>(Text[Position] - '0');
                ++Position;
            }
            return true;
        }

        /// Consumes one exact required property name and its colon separator.
        bool ConsumeProperty(const char* name) {
            std::string actualName;
            return ReadString(actualName) && actualName == name && Consume(':');
        }

        /// Determines whether only whitespace remains after the current value.
        bool AtEnd() {
            SkipWhitespace();
            return Position == Text.size();
        }

    private:
        /// Skips JSON whitespace before reading the next syntax token.
        void SkipWhitespace() {
            while (Position < Text.size() && std::isspace(static_cast<unsigned char>(Text[Position]))) {
                ++Position;
            }
        }

        /// Stores the complete manifest text.
        const std::string& Text;

        /// Stores the next unread byte position.
        std::size_t Position;
    };

    /// Reads one bounded UTF-8 text file into memory.
    bool TryReadTextFile(const std::string& path, std::string& text) {
        std::ifstream stream(path, std::ios::binary);
        if (!stream.good()) {
            return false;
        }

        std::ostringstream content;
        content << stream.rdbuf();
        text = content.str();
        return stream.good() || stream.eof();
    }

    /// Parses one stage object using the host serializer's stable property order.
    bool TryReadStage(JsonCursor& cursor, helengine::psvita::shadercompiler::PsVitaShaderCompilerStage& stage) {
        return cursor.Consume('{')
            && cursor.ConsumeProperty("stageId")
            && cursor.ReadString(stage.StageId)
            && cursor.Consume(',')
            && cursor.ConsumeProperty("sourcePath")
            && cursor.ReadString(stage.SourcePath)
            && cursor.Consume(',')
            && cursor.ConsumeProperty("entryPoint")
            && cursor.ReadString(stage.EntryPoint)
            && cursor.Consume(',')
            && cursor.ConsumeProperty("profile")
            && cursor.ReadString(stage.Profile)
            && cursor.Consume(',')
            && cursor.ConsumeProperty("optionsSignature")
            && cursor.ReadString(stage.OptionsSignature)
            && cursor.Consume('}');
    }

    /// Verifies that an input path remains relative to the compiler inbox.
    bool IsSafeRelativePath(const std::string& path) {
        if (path.empty() || path.front() == '/' || path.front() == '\\' || path.find(':') != std::string::npos) {
            return false;
        }

        return path.find("..") == std::string::npos;
    }

    /// Verifies that an identifier can safely become a fixed outbox directory name.
    bool IsUppercaseHash(const std::string& value) {
        if (value.empty() || value.size() > 128u) {
            return false;
        }

        for (char character : value) {
            bool isDigit = character >= '0' && character <= '9';
            bool isHexLetter = character >= 'A' && character <= 'F';
            if (!isDigit && !isHexLetter) {
                return false;
            }
        }
        return true;
    }
}

namespace helengine::psvita::shadercompiler {
    /// Reads one job manifest from the fixed compiler inbox.
    bool PsVitaShaderCompilerJobReader::TryRead(const std::string& manifestPath, PsVitaShaderCompilerJob& job, std::string& diagnostic) const {
        std::string text;
        if (!TryReadTextFile(manifestPath, text)) {
            diagnostic = "manifest-read-failed";
            return false;
        }
        if (text.empty() || text.size() > 256u * 1024u) {
            diagnostic = "manifest-size-invalid";
            return false;
        }

        JsonCursor cursor(text);
        std::size_t version = 0u;
        PsVitaShaderCompilerJob parsedJob;
        if (!cursor.Consume('{')
            || !cursor.ConsumeProperty("formatVersion")
            || !cursor.ReadUnsigned(version)
            || !cursor.Consume(',')
            || !cursor.ConsumeProperty("jobHash")
            || !cursor.ReadString(parsedJob.JobHash)
            || !cursor.Consume(',')
            || !cursor.ConsumeProperty("stages")
            || !cursor.Consume('[')) {
            diagnostic = "manifest-json-invalid";
            return false;
        }

        if (!cursor.Consume(']')) {
            while (true) {
                PsVitaShaderCompilerStage stage;
                if (!TryReadStage(cursor, stage)) {
                    diagnostic = "manifest-stage-invalid";
                    return false;
                }
                parsedJob.Stages.push_back(stage);
                if (cursor.Consume(']')) {
                    break;
                }
                if (!cursor.Consume(',')) {
                    diagnostic = "manifest-stage-separator-invalid";
                    return false;
                }
            }
        }

        if (!cursor.Consume('}') || !cursor.AtEnd() || version != 1u) {
            diagnostic = "manifest-format-invalid";
            return false;
        }
        if (!Validate(parsedJob, diagnostic)) {
            return false;
        }

        job = parsedJob;
        diagnostic.clear();
        return true;
    }

    /// Validates job values after the schema parser accepted the manifest syntax.
    bool PsVitaShaderCompilerJobReader::Validate(const PsVitaShaderCompilerJob& job, std::string& diagnostic) {
        if (!IsUppercaseHash(job.JobHash) || job.Stages.empty()) {
            diagnostic = "manifest-job-invalid";
            return false;
        }

        std::set<std::string> stageIds;
        for (const PsVitaShaderCompilerStage& stage : job.Stages) {
            if (stage.StageId.empty() || stage.EntryPoint.empty() || stage.OptionsSignature.empty()
                || !IsSafeRelativePath(stage.SourcePath)
                || (stage.Profile != "VP" && stage.Profile != "FP")
                || !stageIds.insert(stage.StageId).second) {
                diagnostic = "manifest-stage-contract-invalid";
                return false;
            }
        }

        return true;
    }
}
