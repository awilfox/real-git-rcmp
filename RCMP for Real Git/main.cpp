//
//  main.cpp
//  RCMP for Real Git
//
//  Created by Andrew Wilcox on 2012-Aug-13.
//  Copyright (c) 2012 Wilcox Technologies LLC. All rights reserved.
//

#include <git2.h>
#include "json/libjson.h"
#include <libAmy/libAmy.h>
#include <errno.h>
#include <iostream>
#include "config.h"
using namespace std;


#ifdef HAVE_PROGINVOC
extern char *program_invocation_short_name;
#endif


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
	cout << "Usage: " << prog_name << " api_endpoint [...]" << endl;
	cout << "\tapi_endpoint\tSend commit info to one or more URLs." << endl;
	cout << endl;
	cout << "Examples:" << endl;
	cout << prog_name << " https://internal.wilcox-tech.com/rcmp" << endl;
	cout << prog_name << " http://rcmp.tenthbit.net/ https://internal/rcmp" << endl;
}


/*!
 \brief determine what changed for this file, and add to json
 \param json		JSONNode for the commit detail
 \param delta		this file's change delta
 \param progress	how slow we're going through the repo
 
 This method is used as a callback to determine the changes of each file and add
 each file to the correct JSON array.
 */
int handle_wtf_changed(const git_diff_delta *delta, float progress, void *json)
{
	const char *path;
	JSONNode *root_node = static_cast<JSONNode *>(json);
	JSONNode actual_node;
	
	switch(delta->status)
	{
		case GIT_DELTA_ADDED:
			actual_node = root_node->pop_back("added");
			path = delta->new_file.path;
			break;
		case GIT_DELTA_MODIFIED:
			actual_node = root_node->pop_back("modified");
			path = delta->new_file.path;
			break;
		case GIT_DELTA_DELETED:
			actual_node = root_node->pop_back("removed");
			path = delta->old_file.path;
			break;
		default:
			return 0;
	}
	
	actual_node.push_back(JSONNode("file", path));
	root_node->push_back(actual_node);
	return 0;
}


/*!
 \brief do stuff with git
 \param path		the path to the git repo
 \param old_id		the old commit ID
 \param new_id		the new commit ID
 \param ref_name	the name of the ref we're parsing (ref/name/master etc)
 
 This is what main() would look like if we didn't have to sanitise because users
 are lusers.
 */
JSONNode *git_hook_main(const char *path, const char *old_id, const char *new_id,
			const char *ref_name)
{
	JSONNode *webhook_node, commit_array(JSON_ARRAY), repo_node;
	git_repository *repo;
	git_oid old_oid, new_oid, last_oid;
	git_revwalk *walker_tx_rgr;
	git_commit *curr_commit;
	
	/* Get the git */
	if(git_repository_open(&repo, path) != 0)
	{
		fprintf(stderr, "Error opening git repository\n");
		return NULL;
	}
	
	
	/* Convert the strings to git oids */
	git_oid_fromstr(&old_oid, old_id);
	git_oid_fromstr(&new_oid, new_id);
	
	
	/* Create our revision walker */
	git_revwalk_new(&walker_tx_rgr, repo);
	git_revwalk_sorting(walker_tx_rgr, GIT_SORT_TIME | GIT_SORT_REVERSE);
	git_revwalk_push(walker_tx_rgr, &new_oid);
	git_revwalk_hide(walker_tx_rgr, &old_oid);
	
	
	/* Set up the basic JSON stuff that won't change */
	webhook_node = new JSONNode;
	webhook_node->push_back(JSONNode("before", old_id));
	webhook_node->push_back(JSONNode("after", new_id));
	webhook_node->push_back(JSONNode("ref", ref_name));
	
	commit_array.set_name("commits");
	
	repo_node.set_name("repository");
	repo_node.push_back(JSONNode("name", "No Name Set"));
	repo_node.push_back(JSONNode("url", git_repository_path(repo)));
	
	JSONNode owner(JSON_NODE);
	owner.set_name("owner");
	owner.push_back(JSONNode("name", "Wilcox Technologies"));
	repo_node.push_back(owner);
	
	// XXX
	// this will never change between refs (at least, it shouldn't)?
	// cache this somehow
	char *path_to_desc = NULL;
	asprintf(&path_to_desc, "%s/description", git_repository_path(repo));
	if(path_to_desc != NULL)
	{
		FILE *desc_file = fopen(path_to_desc, "r");
		if(desc_file != NULL)
		{
			char *repo_desc = static_cast<char *>(malloc(4096));
			if(repo_desc != NULL)
			{
				fread(repo_desc, 4096, 1, desc_file);
				repo_node.push_back(JSONNode("description", repo_desc));
				free(repo_desc);
			}
			
			fclose(desc_file);
		}
		
		free(path_to_desc);
	}
	
	
	// walk commits, adding to array
	last_oid = old_oid;
	
	while((git_revwalk_next(&new_oid, walker_tx_rgr)) == 0)
	{
		JSONNode *commit_details, author_node;
		git_diff_list *diffs;
		git_tree *old_tree, *new_tree;
		git_commit *last_commit;
		char raw_oid[41];
		commit_details = new JSONNode;
		
		if(git_commit_lookup(&curr_commit, repo, &new_oid) != 0)
			continue;
		if(git_commit_lookup(&last_commit, repo, &last_oid) != 0)
			continue;
		
		time_t raw_commit_time = git_commit_time(curr_commit);
		struct tm *time = gmtime(&raw_commit_time);
		char pretty_time[27];
		strftime(pretty_time, 27, "%FT%H:%M:%S-00:00", time);
		
		git_oid_fmt(raw_oid, &new_oid);
		raw_oid[40] = '\0';
		
		commit_details->push_back(JSONNode("id", raw_oid));
		commit_details->push_back(JSONNode("message", git_commit_message(curr_commit)));
		commit_details->push_back(JSONNode("timestamp", pretty_time));
		
		const git_signature *author = git_commit_author(curr_commit);
		author_node.push_back(JSONNode("name", author->name));
		author_node.push_back(JSONNode("email", author->email));
		author_node.set_name("author");
		commit_details->push_back(author_node);
		commit_details->push_back(JSONNode("url", "http://localhost/"));
		
		JSONNode added(JSON_ARRAY), modified(JSON_ARRAY), removed(JSON_ARRAY);
		added.set_name("added");
		modified.set_name("modified");
		removed.set_name("removed");
		
		commit_details->push_back(added);
		commit_details->push_back(modified);
		commit_details->push_back(removed);
		
		// XXX XXX
		// does not check return values of any of the following calls
		git_commit_tree(&old_tree, last_commit);
		git_commit_tree(&new_tree, curr_commit);
		
		git_diff_tree_to_tree(&diffs, repo, old_tree, new_tree, NULL);
		git_diff_foreach(diffs, handle_wtf_changed, NULL, NULL, commit_details);
		git_diff_list_free(diffs);
		// end XXX XXX
		
		commit_array.push_back(*commit_details);
		delete commit_details;
		
		git_commit_free(curr_commit);
		
		last_oid = new_oid;
	}
	
	
	webhook_node->push_back(commit_array);
	webhook_node->push_back(repo_node);
	
	
	/* clean up */
	git_revwalk_free(walker_tx_rgr);
	git_repository_free(repo);
	return webhook_node;
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
	char *git_repo_path;
	char *next_ref;
	vector<WTConnection *> conns;
	
	
	if(argc < 2 || argv[1] == NULL)
	{
#if defined(HAVE_GETPROGNAME)
		usage(getprogname());
#elif defined(HAVE_PROGINVOC)
		usage(program_invocation_short_name);
#else
		usage("real-git-rcmp");
#endif
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
	
	
	for(size_t urls = 1; urls < argc; urls++)
	{
		WTConnection *conn = new WTConnection(NULL);
		conn->connect(argv[urls]);
		conns.push_back(conn);
	}
	
	
	// handle refs passed via stdin
	// format is "old-sha1 SP new-sha1 SP refname LF"
	// so, fgets is a good tool for this
	next_ref = static_cast<char *>(malloc(512));
	while((next_ref = fgets(next_ref, 512, stdin)) != NULL)
	{
		const char *old_id, *new_id; char *ref; JSONNode *node;
		
		char *new_id_start;
		
		// Find the first space
		char *space = strchr(next_ref, (int)' ');
		if(space == NULL) continue;
		
		// Replace the space with NUL so the string is terminated
		space[0] = '\0';
		old_id = next_ref;
		
		// The new SHA1 starts after the space
		new_id_start = space + 1;
		space = strchr(new_id_start, (int)' ');
		if(space == NULL) continue;
		
		// Replace the space with NUL so the string is terminated
		space[0] = '\0';
		new_id = new_id_start;
		
		// The ref is whatever is left over after the new ID's space.
		ref = space + 1;
		
		node = git_hook_main(git_repo_path, old_id, new_id, ref);
		
		if(node == NULL) continue;
		
		for(size_t next_conn = 0; next_conn < conns.size(); next_conn++)
		{
			WTConnection *conn = conns.at(next_conn);
			char *result;
			string payload = "payload=" + string(URLEncode(node->write().c_str()));
			uint64_t len = payload.length();
#ifdef DEBUG
			fprintf(stderr, "POSTing %s (%llu bytes) to %s\n", payload.c_str(), len, argv[next_conn + 1]);
#endif
			result = static_cast<char *>(conn->upload(payload.c_str(), &len));
#ifdef DEBUG
			if(result != NULL)
				fprintf(stderr, "result: %s\n(%llu bytes)", result, len);
#endif
			free(result);
		}
		delete node;
	}
	free(next_ref);
	
	
	while(conns.size() > 0)
	{
		WTConnection *conn = conns.back();
		conn->disconnect();
		delete conn;
		conns.pop_back();
	}
	
	
	free(git_repo_path);
	
	return 0;
}

