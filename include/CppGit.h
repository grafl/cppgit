#ifndef CPPGIT_CPPGIT_H
#define CPPGIT_CPPGIT_H

#include <cstdio>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include <git2.h>

struct merge_options {
    const char** heads;
    size_t heads_count;
    git_annotated_commit** annotated;
    size_t annotated_count;
    unsigned int no_commit : 1;
};

class CppGit {
private:
    static CppGit *instance;
    static std::mutex mutex_;
    CppGit(){};
    static int credentials_cb(git_credential **out, const char *url, const char *username_from_url,
                              unsigned int allowed_types, void *payload);
    static int create_merge_commit(git_repository* repo, git_index* index, struct merge_options* opts);
    static int fetch_update_cb(const char* refname, const git_oid* a, const git_oid* b, void* repo_name);
    static void merge_options_init(struct merge_options* opts);
    static void output_conflicts(git_index* index);
    static int perform_fastforward(git_repository* repo, const git_oid* target_oid, int is_unborn);
    static int resolve_heads(git_repository* repo, struct merge_options* opts);
    static int resolve_refish(git_annotated_commit** commit, git_repository* repo, const char* refish);
protected:
public:
    ~CppGit() = default;
    CppGit(CppGit &other) = delete;
    void operator=(const CppGit&) = delete;
    void operator=(CppGit&&) = delete;
    static CppGit *GetInstance();
    std::unordered_map<std::string, std::vector<std::string>> fetched_commits_to_be_merged;

    static void checkError(char buffer[], int error);
    static int clone(git_repository *repo, char *p_url, char *p_local_path);
    static void fetch(const char* repo_name, git_repository* repo, const char* remote_name);
    static int merge(git_repository* repo, const char* commit_to_merge);
    static void pull(const char* repo_name, const char* url, const char* local_path, const char* remote_name);
};

#endif //CPPGIT_CPPGIT_H
