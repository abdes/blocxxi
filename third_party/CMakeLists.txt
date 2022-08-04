# ===-----------------------------------------------------------------------===#
# Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
# copy at https://opensource.org/licenses/BSD-3-Clause).
# SPDX-License-Identifier: BSD-3-Clause
# ===-----------------------------------------------------------------------===#

message("=> [third-party modules]")

# Live at Head philosophy. Always use the latest commit in the main branch in
# your projects.

# ------------------------------------------------------------------------------
# asap::common
# ------------------------------------------------------------------------------

set(ASAP_COMMON_INSTALL TRUE)
cpmaddpackage(NAME asap_common GIT_TAG master GITHUB_REPOSITORY
              asap-projects/asap-common)

# ------------------------------------------------------------------------------
# GSL
# ------------------------------------------------------------------------------

set(GSL_INSTALL TRUE)
cpmaddpackage(NAME gsl GIT_TAG main GITHUB_REPOSITORY microsoft/GSL)
