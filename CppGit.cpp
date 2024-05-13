#include <cassert>

#include "CppGit.h"

CppGit* CppGit::instance{nullptr};
std::mutex CppGit::mutex_;

int CppGit::create_merge_commit(git_repository *repo, git_index *index, struct merge_options *opts) {
    git_oid tree_oid, commit_oid;
    git_tree* tree;
    git_signature* sign;
    git_reference* merge_ref = nullptr;
    git_annotated_commit* merge_commit;
    git_reference* head_ref;
    auto** parents = (git_commit**)calloc(opts->annotated_count + 1, sizeof(git_commit*));
    const char* msg_target = nullptr;
    size_t msglen;
    char* msg;
    size_t i;
    int err;
    git_repository_head(&head_ref, repo);
    if (resolve_refish(&merge_commit, repo, opts->heads[0])) {
        fprintf(stderr, "failed to resolve refish %s", opts->heads[0]);
        free(parents);
        return -1;
    }
    err = git_reference_dwim(&merge_ref, repo, opts->heads[0]);
    git_signature_now(&sign, "Me", "me@example.com");
#define MERGE_COMMIT_MSG "Merge %s '%s'"
    if (merge_ref != nullptr) {
        git_branch_name(&msg_target, merge_ref);
    }
    else {
        msg_target = git_oid_tostr_s(git_annotated_commit_id(merge_commit));
    }
    msglen = snprintf(nullptr, 0, MERGE_COMMIT_MSG, (merge_ref ? "branch" : "commit"), msg_target);
    if (msglen > 0) msglen++;
    msg = (char*)malloc(msglen);
    err = snprintf(msg, msglen, MERGE_COMMIT_MSG, (merge_ref ? "branch" : "commit"), msg_target);
    if (err < 0) goto cleanup;
    err = git_reference_peel((git_object**)&parents[0], head_ref, GIT_OBJECT_COMMIT);
    for (i = 0; i < opts->annotated_count; i++) {
        git_commit_lookup(&parents[i + 1], repo, git_annotated_commit_id(opts->annotated[i]));
    }
    git_index_write_tree(&tree_oid, index);
    git_tree_lookup(&tree, repo, &tree_oid);
    err = git_commit_create(&commit_oid,
        repo, git_reference_name(head_ref),
        sign, sign,
        nullptr, msg,
        tree,
        opts->annotated_count + 1, parents);
    git_repository_state_cleanup(repo);
cleanup:
    free(parents);
    return err;
}

int CppGit::credentials_cb(git_credential **out, const char *url, const char *username_from_url, unsigned int allowed_types, void *payload) {
    const char *username, *password;
    /*
     * Ask the user via the UI. On error, store the information and return GIT_EUSER which will be
     * bubbled up to the code performing the fetch or push. Using GIT_EUSER allows the application
     * to know it was an error from the application instead of libgit2.
     */
    /* if ((error = ask_user(&username, &password, url, username_from_url, allowed_types)) < 0) {
        store_error(error);
        return GIT_EUSER;
    } */
    username = (char *)"USERNAME";
    password = (char *)"PASSWORD";
    return git_credential_userpass_plaintext_new(out, username, password);
}

int CppGit::fetch_update_cb(const char *refname, const git_oid *a, const git_oid *b, void *repo_name) {
    char a_str[GIT_OID_SHA1_HEXSIZE + 1], b_str[GIT_OID_SHA1_HEXSIZE + 1];
    git_oid_fmt(b_str, b);
    b_str[GIT_OID_SHA1_HEXSIZE] = '\0';
    if (git_oid_is_zero(a)) {
        printf("[new]     %.20s %s\n", b_str, refname);
    } else {
        git_oid_fmt(a_str, a);
        a_str[GIT_OID_SHA1_HEXSIZE] = '\0';
        printf("[updated] %.10s..%s %s\n", a_str, b_str, refname);
    }
    auto it = CppGit::GetInstance()->fetched_commits_to_be_merged.find((char*)repo_name);
    std::vector<std::string> v;
    if (it != CppGit::GetInstance()->fetched_commits_to_be_merged.end()) {
        v = it->second;
        v.emplace_back(b_str);
    }
    else {
        v = { b_str };
    }
    CppGit::GetInstance()->fetched_commits_to_be_merged[(char*)repo_name] = v;
    return 0;
}

void CppGit::merge_options_init(struct merge_options *opts) {
    memset(opts, 0, sizeof(*opts));
    opts->heads = nullptr;
    opts->heads_count = 0;
    opts->annotated = nullptr;
    opts->annotated_count = 0;
}

void CppGit::output_conflicts(git_index* index) {
    git_index_conflict_iterator* conflicts;
    const git_index_entry* ancestor;
    const git_index_entry* our;
    const git_index_entry* their;
    int err;
    git_index_conflict_iterator_new(&conflicts, index);
    while ((err = git_index_conflict_next(&ancestor, &our, &their, conflicts)) == 0) {
        fprintf(stderr, "conflict: a:%s o:%s t:%s\n",
                ancestor ? ancestor->path : "NULL",
                our->path ? our->path : "NULL",
                their->path ? their->path : "NULL");
    }
    if (err != GIT_ITEROVER) {
        fprintf(stderr, "error iterating conflicts\n");
    }
    git_index_conflict_iterator_free(conflicts);
}

int CppGit::perform_fastforward(git_repository* repo, const git_oid* target_oid, int is_unborn) {
    git_checkout_options ff_checkout_options = GIT_CHECKOUT_OPTIONS_INIT;
    git_reference* target_ref;
    git_reference* new_target_ref;
    git_object* target = nullptr;
    int err;
    if (is_unborn) {
        const char* symbolic_ref;
        git_reference* head_ref;
        /* HEAD reference is unborn, lookup manually so we don't try to resolve it */
        err = git_reference_lookup(&head_ref, repo, "HEAD");
        if (err != 0) {
            fprintf(stderr, "failed to lookup HEAD ref\n");
            return -1;
        }
        symbolic_ref = git_reference_symbolic_target(head_ref);
        err = git_reference_create(&target_ref, repo, symbolic_ref, target_oid, 0, nullptr);
        if (err != 0) {
            fprintf(stderr, "failed to create master reference\n");
            return -1;
        }
        git_reference_free(head_ref);
    }
    else {
        err = git_repository_head(&target_ref, repo);
        if (err != 0) {
            fprintf(stderr, "failed to get HEAD reference\n");
            return -1;
        }
    }
    err = git_object_lookup(&target, repo, target_oid, GIT_OBJECT_COMMIT);
    if (err != 0) {
        fprintf(stderr, "failed to lookup OID %s\n", git_oid_tostr_s(target_oid));
        return -1;
    }
    ff_checkout_options.checkout_strategy = GIT_CHECKOUT_SAFE;
    err = git_checkout_tree(repo, target, &ff_checkout_options);
    if (err != 0) {
        fprintf(stderr, "failed to checkout HEAD reference\n");
        return -1;
    }
    err = git_reference_set_target(&new_target_ref, target_ref, target_oid, nullptr);
    if (err != 0) {
        fprintf(stderr, "failed to move HEAD reference\n");
        return -1;
    }
    git_reference_free(target_ref);
    git_reference_free(new_target_ref);
    git_object_free(target);
    return 0;
}

int CppGit::resolve_heads(git_repository* repo, struct merge_options* opts) {
    auto** annotated = (git_annotated_commit**)calloc(opts->heads_count, sizeof(git_annotated_commit*));
    size_t annotated_count = 0, i;
    int err;
    for (i = 0; i < opts->heads_count; i++) {
        err = CppGit::resolve_refish(&annotated[annotated_count++], repo, opts->heads[i]);
        if (err != 0) {
            fprintf(stderr, "failed to resolve refish %s: %s\n", opts->heads[i], git_error_last()->message);
            annotated_count--;
            continue;
        }
    }
    if (annotated_count != opts->heads_count) {
        fprintf(stderr, "unable to parse some refish\n");
        free(annotated);
        return -1;
    }
    opts->annotated = annotated;
    opts->annotated_count = annotated_count;
    return 0;
}

int CppGit::resolve_refish(git_annotated_commit** commit, git_repository* repo, const char* refish) {
    git_reference* ref;
    git_object* obj;
    int err;
    assert(commit != nullptr);
    err = git_reference_dwim(&ref, repo, refish);
    if (err == GIT_OK) {
        git_annotated_commit_from_ref(commit, repo, ref);
        git_reference_free(ref);
        return 0;
    }
    err = git_revparse_single(&obj, repo, refish);
    if (err == GIT_OK) {
        err = git_annotated_commit_lookup(commit, repo, git_object_id(obj));
        git_object_free(obj);
    }
    return err;
}

CppGit *CppGit::GetInstance(){
    if(instance == nullptr) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (instance == nullptr) {
            instance = new CppGit;
        }
        instance->fetched_commits_to_be_merged = {};
    }
    return instance;
}

void CppGit::checkError(char buffer[], int error) {
    if (error < 0) {
        const git_error *e = git_error_last();
        sprintf(buffer, "Error %d/%d: %s\n", error, e->klass, e->message);
        printf("%s\n", buffer);
    }
}

int CppGit::clone(git_repository *p_repo, char *p_url, char *p_local_path) {
    git_clone_options clone_opts = GIT_CLONE_OPTIONS_INIT;
    clone_opts.fetch_opts.callbacks.credentials = CppGit::credentials_cb;
    return git_clone(&p_repo, p_url, p_local_path, &clone_opts);
}

void CppGit::fetch(const char *repo_name, git_repository *repo, const char *remote_name) {
    CppGit::GetInstance()->fetched_commits_to_be_merged[repo_name] = {};
    git_remote* remote = nullptr;
    git_fetch_options fetch_opts = GIT_FETCH_OPTIONS_INIT;
    git_remote_lookup(&remote, repo, remote_name);
    fetch_opts.callbacks.update_tips = &CppGit::fetch_update_cb;
    fetch_opts.callbacks.payload = (void*)repo_name;
    fetch_opts.callbacks.credentials = CppGit::credentials_cb;
    git_remote_fetch(remote, nullptr, &fetch_opts, "fetch");
    git_remote_free(remote);
}

int CppGit::merge(git_repository *repo, const char *commit_to_merge) {
    struct merge_options opts{};
    git_index* index;
    git_repository_state_t state;
    git_merge_analysis_t analysis;
    git_merge_preference_t preference;
    int err;
    CppGit::merge_options_init(&opts);
    opts.heads = &commit_to_merge;
    opts.heads_count = 1;
    state = (git_repository_state_t)git_repository_state(repo);
    if (state != GIT_REPOSITORY_STATE_NONE) {
        fprintf(stderr, "repository is in unexpected state %d\n", state);
        free((char**)opts.heads);
        free(opts.annotated);
    }
    err = CppGit::resolve_heads(repo, &opts);
    if (err != 0) {
        free((char**)opts.heads);
        free(opts.annotated);
    }
    err = git_merge_analysis(&analysis, &preference,
                             repo,
                             (const git_annotated_commit**)opts.annotated,
                             opts.annotated_count);
    if (analysis & GIT_MERGE_ANALYSIS_UP_TO_DATE) {
        return 0;
    } else if (analysis & GIT_MERGE_ANALYSIS_UNBORN ||
             (analysis & GIT_MERGE_ANALYSIS_FASTFORWARD &&
              !(preference & GIT_MERGE_PREFERENCE_NO_FASTFORWARD))) {
        const git_oid* target_oid;
        target_oid = git_annotated_commit_id(opts.annotated[0]);
        assert(opts.annotated_count == 1);
        return CppGit::perform_fastforward(repo, target_oid, (analysis & GIT_MERGE_ANALYSIS_UNBORN));
    } else if (analysis & GIT_MERGE_ANALYSIS_NORMAL) {
        git_merge_options merge_opts = GIT_MERGE_OPTIONS_INIT;
        git_checkout_options checkout_opts = GIT_CHECKOUT_OPTIONS_INIT;
        merge_opts.flags = 0;
        merge_opts.file_flags = GIT_MERGE_FILE_STYLE_DIFF3;
        checkout_opts.checkout_strategy = GIT_CHECKOUT_FORCE | GIT_CHECKOUT_ALLOW_CONFLICTS;
        if (preference & GIT_MERGE_PREFERENCE_FASTFORWARD_ONLY) {
            return -1;
        }
        err = git_merge(repo,
                        (const git_annotated_commit**)opts.annotated, opts.annotated_count,
                        &merge_opts, &checkout_opts);
    }
    git_repository_index(&index, repo);
    if (git_index_has_conflicts(index)) {
        CppGit::output_conflicts(index);
    } else if (!opts.no_commit) {
        CppGit::create_merge_commit(repo, index, &opts);
    }
    return 0;
}

void CppGit::pull(const char *repo_name, const char *url, const char *local_path, const char *remote_name) {
    git_repository* repo;
    git_repository_open(&repo, local_path);
    CppGit::fetch(repo_name, repo, remote_name);
    std::vector<std::string> commits_to_merge = CppGit::GetInstance()->fetched_commits_to_be_merged.find(repo_name)->second;
    printf("Fetch done. %zu new object%s ha%s been fetched.\n",
           commits_to_merge.size(), (!commits_to_merge.empty() ? "s" : ""), (!commits_to_merge.empty() ? "ve" : "s"));
    for (const std::string& commit_to_merge : commits_to_merge) {
        git_oid oid;
        git_oid_fromstr(&oid, commit_to_merge.c_str());
        git_object* obj;
        git_object_lookup(&obj, repo, &oid, GIT_OBJECT_ANY);
        if (git_object_type(obj) == GIT_OBJECT_COMMIT) {
            printf("Merging: %s - %s", (char*)repo_name, commit_to_merge.c_str());
            CppGit::merge(repo, commit_to_merge.c_str());
        }
    }
    git_repository_free(repo);
}
