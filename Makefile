# Fleet Dispatcher — monorepo convenience targets.
#
# The mobile app (portals/mobile) is developed here and published to the
# standalone Fleet-Dispatcher-Mobile repo via git subtree (VCP builds that repo).

# Override the mobile repo URL if it differs from the default:
#   make publish-mobile MOBILE_REMOTE_URL=https://github.com/thomasgpeters/Fleet-Dispatcher-Mobile.git
MOBILE_REMOTE_URL ?= https://github.com/thomasgpeters/Fleet-Dispatcher-Mobile.git
MOBILE_REMOTE_NAME ?= mobile
MOBILE_BRANCH ?= main
MOBILE_PREFIX := portals/mobile

.PHONY: help publish-mobile pull-mobile mobile-build

help:
	@echo "Targets:"
	@echo "  publish-mobile   Push portals/mobile -> Fleet-Dispatcher-Mobile (git subtree)"
	@echo "  pull-mobile      Pull changes made in Fleet-Dispatcher-Mobile back into portals/mobile"
	@echo "  mobile-build     npm install + build the mobile app locally"

# Publish the mobile subtree to its standalone repo.
publish-mobile:
	MOBILE_REMOTE_URL=$(MOBILE_REMOTE_URL) \
	MOBILE_REMOTE_NAME=$(MOBILE_REMOTE_NAME) \
	MOBILE_BRANCH=$(MOBILE_BRANCH) \
	scripts/publish-mobile.sh

# Pull changes that were made directly in the standalone repo back into the
# monorepo (squashed). Use if anyone commits to Fleet-Dispatcher-Mobile directly.
pull-mobile:
	git remote get-url $(MOBILE_REMOTE_NAME) >/dev/null 2>&1 || \
		git remote add $(MOBILE_REMOTE_NAME) $(MOBILE_REMOTE_URL)
	git subtree pull --prefix=$(MOBILE_PREFIX) $(MOBILE_REMOTE_NAME) $(MOBILE_BRANCH) --squash

# Local build sanity check for the mobile module.
mobile-build:
	cd $(MOBILE_PREFIX) && npm install && npm run build
