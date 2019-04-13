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

#include "api/masterlist.h"

#include "api/game/game.h"
#include "api/helpers/git_helper.h"
#include "api/helpers/logging.h"
#include "loot/exception/file_access_error.h"
#include "loot/exception/git_state_error.h"

using std::string;

namespace fs = std::filesystem;

namespace loot {
MasterlistInfo Masterlist::GetInfo(const std::filesystem::path& path,
                                   bool shortID) {
  // Compare HEAD and working copy, and get revision info.
  GitHelper git;
  MasterlistInfo info;

  auto logger = getLogger();

  if (!fs::exists(path)) {
    if (logger) {
      logger->info("Unknown masterlist revision: No masterlist present.");
    }
    throw FileAccessError(string("N/A: No masterlist present at ") +
                          path.u8string());
  } else if (!git.IsRepository(path.parent_path())) {
    if (logger) {
      logger->info("Unknown masterlist revision: Git repository missing.");
    }
    throw GitStateError(string("Unknown: \"") + path.parent_path().u8string() +
                        "\" is not a Git repository.");
  }

  git.Open(path.parent_path());

  info.revision_id = git.GetHeadCommitId(shortID);
  info.revision_date = git.GetHeadCommitDate();

  if (logger) {
    logger->trace("Diffing masterlist HEAD and working copy.");
  }
  info.is_modified =
      GitHelper::IsFileDifferent(path.parent_path(), path.filename().u8string());

  return info;
}

bool Masterlist::IsLatest(const std::filesystem::path& path,
                          const std::string& repoBranch) {
  if (repoBranch.empty())
    throw std::invalid_argument("Repository branch must not be empty.");

  GitHelper git;
  auto logger = getLogger();

  if (!git.IsRepository(path.parent_path())) {
    if (logger) {
      logger->info(
          "Cannot get latest masterlist revision: Git repository missing.");
    }
    throw GitStateError(string("Unknown: \"") + path.parent_path().u8string() +
                        "\" is not a Git repository.");
  }

  git.Open(path.parent_path());

  git.Fetch("origin");

  return git.BranchExists(repoBranch) && git.IsBranchUpToDate(repoBranch) &&
         git.IsBranchCheckedOut(repoBranch);
}

bool Masterlist::Update(const std::filesystem::path& path,
                        const std::string& repoUrl,
                        const std::string& repoBranch) {
  GitHelper git;
  auto logger = getLogger();
  fs::path repoPath = path.parent_path();
  string filename = path.filename().u8string();

  if (path.empty() || repoUrl.empty() || repoBranch.empty())
    throw std::invalid_argument("Repository path, URL and branch must not be empty.");

  git.InitialiseOptions(repoBranch, filename);

  // Now try to access the repository if it exists, or clone one if it doesn't.
  if (logger) {
    logger->trace("Checking for Git repository at: {}", repoPath.u8string());
  }
  if (!GitHelper::IsRepository(repoPath)) {
    git.Clone(repoPath, repoUrl);
  } else {
    git.Open(repoPath);

    // Set the remote URL. This assumes a single-URL remote called "origin"
    // exists.
    git.SetRemoteUrl("origin", repoUrl);

    // Now fetch updates from the remote.
    git.Fetch("origin");

    if (logger) {
      logger->debug(
          "Checking if branch {} is up to date and checked out without edits",
          repoBranch);
    }

    if (git.BranchExists(repoBranch)) {
      if (git.IsBranchUpToDate(repoBranch) &&
          git.IsBranchCheckedOut(repoBranch) &&
          !GitHelper::IsFileDifferent(repoPath, filename)) {
        if (logger) {
          logger->info(
              "Local branch and masterlist file are already up to date.");
        }
        return false;
      }

      git.DeleteBranch(repoBranch);
    }

    // No local branch exists, create and checkout a new one from the remote.
    git.CheckoutNewBranch("origin", repoBranch);
  }

  // Now whether the repository was cloned or updated, the working directory
  // contains the latest masterlist. Try parsing it: on failure, detach the HEAD
  // back one commit and try again.
  while (true) {
    try {
      this->Load(path);

      return true;
    } catch (std::exception& e) {
      if (logger) {
        logger->error("Masterlist parsing failed. Masterlist revision {}: {}",
                      git.GetHeadCommitId(true),
                      e.what());
      }
      git.CheckoutRevision("HEAD^");
    }
  }

  // This should never be reached as git.CheckoutRevision() will throw if it
  // tries to go one back from the start of history.
  return true;
}
}
