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

# ALS project to install our extensions into (override as needed):
#   make als-extensions ALS_PROJECT=/path/to/fleet-dispatcher-api
ALS_PROJECT ?= ../fleet-dispatcher-api

.PHONY: help publish-mobile pull-mobile mobile-build als-extensions

help:
	@echo "Targets:"
	@echo "  publish-mobile   Push portals/mobile -> Fleet-Dispatcher-Mobile (git subtree)"
	@echo "  pull-mobile      Pull changes made in Fleet-Dispatcher-Mobile back into portals/mobile"
	@echo "  mobile-build     npm install + build the mobile app locally"
	@echo "  als-extensions   Install ALS Kafka-event logic into \$$ALS_PROJECT (run after ALS create)"

# Install our auto-discovered ALS customizations (Kafka event producers) into a
# generated ApiLogicServer project. Run after `ApiLogicServer create`; rebuilds
# preserve logic/, so this is a one-time step per fresh generate.
als-extensions:
	als-extensions/install.sh "$(ALS_PROJECT)"

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
