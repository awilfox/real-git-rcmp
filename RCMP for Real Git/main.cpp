//
//  main.cpp
//  RCMP for Real Git
//
//  Created by Andrew Wilcox on 2012-Aug-13.
//  Copyright (c) 2012 Wilcox Technologies LLC. All rights reserved.
//

#include <git2.h>
#include <errno.h>
#include <iostream>
using namespace std;



/*!
 http://bytes.com/topic/c/answers/216710-does-ansi-c-have-something-like-pathmax-max_path
 
 windows can be anywhere from 260 to 32000 bytes
 HP-UX thinks it can only handle 14 chars just because it read UFS1
 OpenBSD thinks it can only handle 1024, unless it's on NFS
 so, screw yo' constant
 */
#define MAGIC_PATH_SIZE_BECAUSE_EVERY_OS_SUCKS	32768



#ifndef I_AM_NOT_SANE
#	define PATH_LEN MAGIC_PATH_SIZE_BECAUSE_EVERY_OS_SUCKS
#else /* you aren't sane */
#	include <cstdio>
#	define PATH_LEN FILENAME_MAX
#endif



/*!
 \brief print usage
 \param prog_name	the name of the executable
 
 Prints the argument syntax for the executable.
 */
void usage(const char *prog_name)
{
	cout << prog_name << " - RCMP for Real Git" << endl;
	cout << endl;
	cout << "Usage: " << prog_name << " api_endpoint" << endl;
	cout << "\tapi_endpoint\tSend commit info to this URL." << endl;
	cout << endl;
	cout << "Example:" << endl;
	cout << prog_name << " https://internal.wilcox-tech.com/rcmp/#spark" << endl;
}



/*!
 \brief do stuff with git
 \param path		the path to the git repo
 \param old_id		the old commit ID
 \param ref_name	the name of the ref we're parsing (ref/name/master etc)
 \param url		URL to post to
 
 This is what main() would look like if we didn't have to sanitise because users
 are lusers.
 */
int git_hook_main(const char *path, const char *old_id, const char *ref_name,
		  const char *url)
{
	git_repository *repo;
	git_oid base_oid;
	git_revwalk *walker_tx_rgr;
	git_commit *curr_commit;
	
	if(git_repository_open(&repo, path) != 0)
	{
		fprintf(stderr, "Error opening git repository\n");
		return -1;
	}
	
	git_oid_fromstr(&base_oid, old_id);
	
	git_revwalk_new(&walker_tx_rgr, repo);
	git_revwalk_sorting(walker_tx_rgr, GIT_SORT_TIME | GIT_SORT_REVERSE);
	git_revwalk_push(walker_tx_rgr, &base_oid);
	
	
	
	git_revwalk_free(walker_tx_rgr);
	
	git_repository_free(repo);
	return 0;
}


/*!
 \brief whereis .git
 \param path		the path to start from
 
 Traverse directories looking for the git repo.  Since we're starting in the
 hooks dir (more than likely), it won't be '.'.
 */
char *find_git_repo_from_path(const char *path)
{
	char *real_git_path = static_cast<char *>(malloc(PATH_LEN));
	int result = git_repository_discover(real_git_path, PATH_LEN,
					     path, false, NULL);
	
	if(result != 0)
	{
		free(real_git_path);
		real_git_path = NULL;
	}
	
	return real_git_path;
}



/*!
 \brief figure out the meaning of life
 \param argc		number of args
 \param argv		arguments
 
 Parses arguments and environment variables and kicks off execution of our
 little git hook.
 */
int main(int argc, const char * argv[])
{
	char *git_repo_path = NULL;
	int ret;
	
	
	if(argc != 2 || argv[1] == NULL)
	{
		usage(getprogname());
		return -1;
	}
	
	
	// Why set the errno?
	// because, if we don't have a GIT_DIR, we use cwd.
	// if cwd (on up) doesn't have a git repo, then we won't have an errno
	//   to print for perror and it might be something silly, like
	//   "Not a typewriter".  let's not do that, mmk?
	errno = ENOENT;
	
	if(getenv("GIT_DIR") == NULL)
	{
		char *cwd = getcwd(NULL, 0);
		git_repo_path = find_git_repo_from_path(cwd);
		free(cwd);
	}
	else
		git_repo_path = strdup(getenv("GIT_DIR"));
	
	
	if(git_repo_path == NULL)
	{
		perror("can't find git path");
		return 1;
	}
	
	
	// HOW READ FROM STDIN?!
	ret = git_hook_main(git_repo_path, NULL, NULL, argv[1]);
	free(git_repo_path);
	
	return ret;
}

