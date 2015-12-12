#include <iostream>
#include <fuse.h>
#include <unistd.h>
#include <string>
#include <getopt.h>
#include "GamecubeIsoFilesystem.h"

using namespace std;

static const struct option long_opts[] = {
    {"uid", required_argument, NULL, 'u'},
    {"gid", required_argument, NULL, 'g'},
    {"logfile", required_argument, NULL, 'l'},
    {"iso", required_argument, NULL, 'i'},
    {"mount_point", required_argument, NULL, 'm'},
    {"help", no_argument, NULL, 'h'},
    {0, 0, 0, 0},
};

int printHelp() {
  printf("Mounts a Gamecube ISO file\n\n"
         "    -u, --uid                 uid of files\n"
         "    -g, --gid                 gid of files\n"
         "    -l, --logfile=file        debug logfile location\n"
         "    -i, --iso=file            Gamecube ISO file location\n"
         "    -m, --mount_point=file    mount point\n"
         "    -h, --help                this help menu\n");

  return 0;
}

int main(int argc, char **argv) {
  char ch;
  uid_t uid = geteuid();
  gid_t gid = getegid();
  string isoFile;
  string logFile;
  string mountPoint;
  GamecubeIsoFilesystem *context;

  if (getuid() == 0 || uid == 0) {
    cerr << "Can't run as root" << endl;
    return 1;
  }

  while ((ch = getopt_long(argc, argv, "ugl:i:m:h", long_opts, NULL)) != -1) {
    switch (ch) {
    case 'u':
      uid = atol(optarg);
      break;
    case 'g':
      gid = atol(optarg);
      break;
    case 'l':
      logFile = optarg;
      break;
    case 'i':
      isoFile = optarg;
      break;
    case 'm':
      mountPoint = optarg;
      break;
    case 'h':
      return printHelp();
    }
  }

  if (isoFile.empty()) {
    fprintf(stderr, "You need to specify an iso file!\n");
    return 1;
  }
  if (mountPoint.empty()) {
    fprintf(stderr, "You need to specify a mount point!\n");
    return 2;
  }

  context = new GamecubeIsoFilesystem(uid, gid, logFile.c_str());
  if (context->open(isoFile.c_str())) {
    // create fake argc, argv for fuse
    char *fake_argv[2] = {argv[0], const_cast<char *>(mountPoint.c_str())};
    return fuse_main(sizeof(fake_argv) / sizeof(fake_argv[0]), fake_argv,
                     context->getFuseOperations(), context);
  } else {
    fprintf(stderr, "Unable to open %s\n", isoFile.c_str());
    delete context;
    return 3;
  }
}
