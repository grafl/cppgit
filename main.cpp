#include <filesystem>

#include "CppGit.h"

int main(int argc, char *argv[]) {
    char buffer[128];
    git_libgit2_init();
    CppGit::GetInstance();
    const char *url = "https://HOST/repo.git";
    const char *local_path = "c:\\repo";
    git_repository *repo = nullptr;

    /* if(std::filesystem::exists(local_path)) {
        std::filesystem::remove_all(local_path);
    }

    int rv = CppGit::clone(repo, const_cast<char *>(url), const_cast<char *>(local_path));
    CppGit::checkError(buffer, rv); */

    CppGit::pull("REPO", url, local_path, "origin");

    git_repository_free(repo);
    git_libgit2_shutdown();
    return 0;
}
