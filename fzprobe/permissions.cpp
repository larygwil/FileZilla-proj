#include "permissions.h"
#include "log.h"
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
#include <errno.h>

static int cached_root = -1;
bool is_root()
{
  if (cached_root == -1)
    cached_root = (getuid() == 0) ? 1 : 0;
  return cached_root != 0;
}

bool drop_root()
{
  static const char* username = "nobody";
  struct passwd* pwd = getpwnam(username);
  if (!pwd) {
    logger.Log("Failed to drop root privileges, getpwnam failed: %d\n", errno);
    return false;
  }
  logger.Log("Dropping root privileges to %s (%d)\n", username, pwd->pw_uid);
  if (setresgid(pwd->pw_uid, pwd->pw_uid, pwd->pw_uid) == -1) {
    logger.Log("Failed to drop root privileges, setresgid failed: %d\n", errno);
    return false;
  }
  if (setresuid(pwd->pw_uid, pwd->pw_uid, pwd->pw_uid) == -1) {
    logger.Log("Failed to drop root privileges, setresuid failed: %d\n", errno);
    return false;
  }
  return true;
}


