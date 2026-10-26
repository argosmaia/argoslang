#include "argos/driver/native_compile.hpp"

#include "argos/codegen/native/x86_64/x86_64_codegen.hpp"

#include <fstream>
#include <stdexcept>
#include <string>

#if defined(__linux__) && defined(__x86_64__)
#include <cerrno>
#include <cstring>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
extern char **environ;
#endif

namespace argos::driver {
int emit_assembly(const argos::ir::Module &module,
                  const std::filesystem::path &output,
                  std::string &error) {
  std::string assembly;
  try {
    codegen::native::x86_64::CodeGenerator backend;
    assembly = backend.generate(module);
  } catch (const std::exception &exception) {
    error = exception.what();
    return 65;
  }
  std::ofstream file{output, std::ios::binary};
  file << assembly;
  file.close();
  if (!file) {
    error = "não foi possível gravar assembly: " + output.string();
    return 74;
  }
  return 0;
}

int compile_native(const argos::ir::Module &module,
                   const std::filesystem::path &output,
                   std::string &error) {
#if defined(__linux__) && defined(__x86_64__)
  std::string assembly;
  try {
    codegen::native::x86_64::CodeGenerator backend;
    assembly = backend.generate(module);
  } catch (const std::exception &exception) {
    error = exception.what();
    return 65;
  }

  std::string temporary =
      (std::filesystem::temp_directory_path() / "argosc-assembly-XXXXXX").string();
  const int descriptor = mkstemp(temporary.data());
  if (descriptor < 0) {
    error = "não foi possível criar arquivo temporário: " + std::string{std::strerror(errno)};
    return 74;
  }
  close(descriptor);
  struct TemporaryFile final {
    std::string path;
    ~TemporaryFile() { unlink(path.c_str()); }
  } cleanup{temporary};
  {
    std::ofstream file{temporary, std::ios::binary};
    file << assembly;
    if (!file) {
      error = "não foi possível gravar assembly temporário";
      return 74;
    }
  }

  const auto output_directory = output.has_parent_path() ? output.parent_path()
                                                        : std::filesystem::path{"."};
  std::string temporary_output =
      (output_directory / ".argosc-output-XXXXXX").string();
  const int output_descriptor = mkstemp(temporary_output.data());
  if (output_descriptor < 0) {
    error = "não foi possível criar saída temporária: " + std::string{std::strerror(errno)};
    return 74;
  }
  close(output_descriptor);
  unlink(temporary_output.c_str());
  TemporaryFile output_cleanup{temporary_output};
  char *arguments[] = {const_cast<char *>("cc"), const_cast<char *>("-x"),
                       const_cast<char *>("assembler"), temporary.data(),
                       const_cast<char *>("-o"),
                       temporary_output.data(), nullptr};
  pid_t process{};
  const int spawn_error = posix_spawnp(&process, "cc", nullptr, nullptr, arguments, environ);
  if (spawn_error != 0) {
    error = "não foi possível iniciar cc: " + std::string{std::strerror(spawn_error)};
    return 69;
  }
  int status{};
  if (waitpid(process, &status, 0) < 0 || !WIFEXITED(status) || WEXITSTATUS(status) != 0) {
    error = "assembler/linker falhou; confira as mensagens de cc";
    return 69;
  }
  std::error_code rename_error;
  std::filesystem::rename(temporary_output, output, rename_error);
  if (rename_error) {
    error = "não foi possível instalar o executável: " + rename_error.message();
    return 74;
  }
  return 0;
#else
  (void)module;
  (void)output;
  error = "compilação nativa disponível somente em x86-64/Linux nesta versão";
  return 69;
#endif
}
} // namespace argos::driver
