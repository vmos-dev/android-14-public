#include <unistd.h>

#define LOG_TAG "wait_for_optee"
#include <android-base/logging.h>

#include <android/binder_manager.h>
#include <aidl/android/hardware/security/keymint/IKeyMintDevice.h>
#include <keymasterV4_1/Keymaster.h>

using android::hardware::keymaster::V4_1::SecurityLevel;
using android::hardware::keymaster::V4_1::support::Keymaster;
using aidl::android::hardware::security::keymint::IKeyMintDevice;

useconds_t kWaitTimeMicroseconds = 1 * 1000;  // 1 milliseconds

int main() {
    for (unsigned cycleCount = 0; /* Forever */; ++cycleCount) {
        // Check keymint
        const std::string keymintDesc = std::string() + IKeyMintDevice::descriptor + "/default";
        auto keymint = IKeyMintDevice::fromBinder(
                ndk::SpAIBinder(AServiceManager_waitForService(keymintDesc.c_str())));
        if (keymint != nullptr) {
            LOG(INFO) << "TEE keymint is ready";
            return 0;
        }
        LOG(INFO) << "Get back to check keymaster 3/4";

        // Check keymaster
        auto keymasters = Keymaster::enumerateAvailableDevices();

        for (auto &dev : keymasters) {
            SecurityLevel securityLevel = dev->halVersion().securityLevel;
            if (securityLevel == SecurityLevel::TRUSTED_ENVIRONMENT) {
                LOG(INFO) << "TEE keymaster is ready";
                return 0;
            } else if (securityLevel == SecurityLevel::SOFTWARE) {
                LOG(INFO) << "Software keymaster is ready";
                return 0;
            }
        }

        if (cycleCount % 10 == 1) {
            LOG(WARNING) << "Still waiting for TEE";
        }
        usleep(kWaitTimeMicroseconds);
    }
}
