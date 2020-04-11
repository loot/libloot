/*  LOOT

    A load order optimisation tool for Oblivion, Skyrim, Fallout 3 and
    Fallout: New Vegas.

    Copyright (C) 2012-2016    WrinklyNinja

    This file is part of LOOT.

    LOOT is free software: you can redistribute
    it and/or modify it under the terms of the GNU General Public License
    as published by the Free Software Foundation, either version 3 of
    the License, or (at your option) any later version.

    LOOT is distributed in the hope that it will
    be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with LOOT.  If not, see
    <https://www.gnu.org/licenses/>.
    */

#ifndef LOOT_API_HELPERS_GIT_HELPER
#define LOOT_API_HELPERS_GIT_HELPER

#include <filesystem>
#include <string>

#include <git2.h>
#include <spdlog/spdlog.h>

namespace loot {
class GitHelper {
public:
  void InitialiseOptions(const std::string& branch,
                         const std::string& filenameToCheckout);
  void Open(const std::filesystem::path& repoRoot);
  void SetRemoteUrl(const std::string& remote, const std::string& url);

  static bool IsRepository(const std::filesystem::path& path);
  static bool IsFileDifferent(const std::filesystem::path& repoRoot,
                              const std::string& filename);

  void Clone(const std::filesystem::path& path, const std::string& url);
  void Fetch(const std::string& remote);

  void CheckoutNewBranch(const std::string& remote, const std::string& branch);
  void CheckoutRevision(const std::string& revision);

  // Deletes the branch, detaching HEAD if it's currently set to the branch.
  void DeleteBranch(const std::string& branch);

  bool BranchExists(const std::string& branch);
  bool IsBranchUpToDate(const std::string& branch);
  bool IsBranchCheckedOut(const std::string& branch);

  std::string GetHeadCommitId(bool shortId);
  std::string GetHeadCommitDate();

private:
  struct DiffPayload {
    bool fileFound;
    const char* fileToFind;
  };

  struct GitData {
    GitData();
    ~GitData();

    git_repository* repo;
    git_remote* remote;
    git_config* config;
    git_object* object;
    git_commit* commit;
    git_reference* reference;
    git_reference* reference2;
    git_blob* blob;
    git_annotated_commit* annotated_commit;
    git_tree* tree;
    git_diff* diff;
    git_buf buffer;

    git_checkout_options checkout_options;
    git_clone_options clone_options;
  };

  static int DiffFileCallback(const git_diff_delta* delta,
                              float progress,
                              void* payload);

  // Removes the read-only flag from some files in git repositories
  // created by libgit2.
  void GrantWritePermissions(const std::filesystem::path& path);

  void Call(int error_code);

  const git_oid* GetCommitId(git_reference* reference);

  GitData data_;
};
}
#endif
