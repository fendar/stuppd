#include "include/ss_core.h"

#define SS_ERRLOG_NAME      "log/errlog.log"
#define SS_CONFIG_FILE      "conf/stuppd.conf"
#define SS_DEFAULT_WWWPATH  "www"

//global variable
ss_log_t    errlog;
ss_file_t   logfile;
ss_map_t    *fileconf;
ss_listen_t listening;
ss_str_t    wwwpath;

//static global variable
static char *workdir;

//static global function
static ss_int_t ss_get_dir(char *arg, char **workdirp);
static ss_int_t ss_log_init(); 
static ss_int_t ss_parse_config();//process the config from config file
static ss_int_t ss_check_config();//check if the config is correct and merge the config

int main(int argc, char *argv[])
{
    ss_map_t    *tmp;
    ss_int_t    port;

    //step 1:get the stuppd workpath
    if (ss_get_dir(argv[0], &workdir) == -1) {
        ss_stderr_log("ss_get_dir() occurs error\n");
        return -1;
    }
    //step 2:init the log
    if (ss_log_init() == -1) {
        ss_stderr_log("ss_log_init() occurs error\n");
        return -1;
    }
    
    //step 3: parse the config from stuppd.conf
    fileconf = NULL;
    if (ss_parse_config() == -1) {
        ss_stderr_log("ss_parse_config() occurs error\n");
        return -1;
    }
    if (ss_check_config() == -1) {
        /*ss_stderr_log("ss_check_config() error\n");*/
        return -1;
    }
    //step 4: socket process,
        //find the port that want to listen
    tmp = fileconf;
    port = 0; 
    while (tmp != NULL) {
        if (strcmp((const char *)tmp->key, "listen") == 0) {
            port = atoi(tmp->value);
            break;
        }
    }
    memset(&listening, (char)0 , sizeof(ss_listen_t));
    (port <= 0) && (port = 8080);// in c language, the "=" will function after "&&"
    listening.port = port;
        //init the listen socket
    if (ss_listen_init(&listening) == -1) {
        ss_stderr_log("ss_listen_init() error\n");
        return -1;
    } 
    //step 5:   to be a deamon;
    if (ss_daemonize() == -1) {
        ss_stderr_log("ss_deamonize() ocurrs error\n");
        return -1;
    }

    //step 6:   start to handle event
    ss_event_cycle(&listening, &errlog);
    return 0;
}

static ss_int_t
ss_log_init()
{
    ss_int_t    dlen, nlen, logfd;
    char        *name;

    dlen = strlen(workdir);
    nlen = strlen(SS_ERRLOG_NAME);
        
    name = (char *)malloc(dlen + nlen + 2);// +2 because "/" and '\0'
    if (!name) {
        return -1;
    }
    memcpy(name, workdir, dlen);
    memcpy(name + dlen, "/"SS_ERRLOG_NAME, nlen + 1);
    name[dlen+nlen+1] = '\0';
        
    //open the logfile
    logfd = ss_open_file(name, SS_FILE_WRONLY, SS_FILE_APPEND | SS_FILE_OPEN_OR_CREAT, SS_FILE_ACCESS);
    if (logfd != -1) {
        logfile.fd = logfd;
        logfile.flag = SS_FILE_WRONLY;
        logfile.filename.data = name;
        logfile.filename.len = (name[nlen] == '\0' ? nlen : dlen + nlen + 1);
        errlog.file = &logfile;
        return 0;
    } else {
        ss_stderr_log("SSLOG:open logfile(%s) error, strerror:%s", name, strerror(errno));
        return -1;
    }
}

//there is still lots of problems of parsing config,but now i skip...
static ss_int_t
ss_parse_config()
{
    char        buf[1025];
    char        *file = NULL, *k = NULL;
    ss_int_t    filefd, dlen, clen, i, klen, flag;
    ssize_t     nread;
    ss_map_t    filemap, *tmpmap;

    dlen = strlen(workdir);
    clen = strlen(SS_CONFIG_FILE);
    file = (char *)malloc(dlen + clen + 2); //1 for '/' one for '\0'
    if (!file) {
        return -1;
    }
    memcpy(file, workdir, dlen);
    memcpy(file + dlen, "/"SS_CONFIG_FILE, clen + 1);
    file[dlen + clen + 1] = '\0';

    //open conf file
    if ((filefd = ss_open_file(file, SS_FILE_RDONLY, SS_FILE_RDONLY, SS_FILE_ACCESS)) == -1) {
        goto merror;
    } //don`t set mode, can use flag replace

    //if read bytes beyond 1024,return -1
    if ((nread = read(filefd, buf, 1024)) == 1024 || nread == -1) {
        //ss_stderr_log("conf file is too big\n");
        goto merror;
    }
    ss_close_file(filefd);
    //start to parse the config in buf
    //every line only sets one config, handle word by word,line start identified by flag
    filemap.key = filemap.value = NULL;
    filemap.next = NULL;
    flag = 0;
    for (i = 0; i < nread; /*void*/) {
        flag = !flag;
        klen =  0;
        k =  NULL;
        //ignore the space
        while (buf[i] == ' ')
            ++i;
        if (i >= nread || buf[i] == '\n') {
            flag = 0;
            ++i;
            continue;
        }
        //now handle the key or value
        while (i < nread && buf[i] != ' ' && buf[i] != '\n') {
            ++i;
            ++klen;
        }
        k = (char *)malloc(klen + 1);
        if (!k)
            goto merror;
        memcpy(k, buf + i - klen, klen);
        k[klen] = '\0';
        //if handle key,only interested for "listen" "wwwpath" "404" "403" "500"
        if (flag) {
            //whether strncmp is fit?Maybe there is a better way to replace it;
            if (klen == 6 && strncmp("listen", k, 6) == 0) {
                /*void*/
            } else if (klen == 7 && strncmp("wwwpath", k, 7) == 0) {
                /*void*/
            } else if (klen == 3 && (strncmp("404", k, 3) == 0 || strncmp("403", k, 3) == 0 || strncmp("500", k ,3) ==0)) {
                /*void*/
            } else {
                //ignore current line
                flag = 0;
                while (i < nread && buf[i] != '\n')
                    ++i;
                continue;
            }
            //k is the key, filemap is a quenue and the head don`t store data
            tmpmap = &filemap;
            while (tmpmap->next)
                tmpmap = tmpmap->next;
            tmpmap->next = (ss_map_t *)malloc(sizeof(ss_map_t));
            if (tmpmap->next == NULL)
                goto merror;
            tmpmap = tmpmap->next;
            tmpmap->key = k;
        } else {
            //k is the value
            tmpmap->value = k;
        }
    }
    fileconf = filemap.next;
    return 0;
//in fact, if ss_parse_config() return -1,then the process will end,so we can don`t free the memory;
merror:
    free(file);
    if (k != NULL)
        free(k);
    return -1;
}

static ss_int_t
ss_get_dir(char *arg, char **dirnamep)
{
    ss_int_t    dlen, rlen;
    char        *name, *ret;

    dlen = strlen(arg);
    name = (char *)malloc(dlen + 1);
    if (!name) {
        return -1;
    }
    memcpy(name, arg, dlen);
    name[dlen] = '\0';
    //because dirname(3) may change the arg,so I copy the argv[0] to name;
    ret = dirname(name);
    rlen = strlen(ret);
    *dirnamep = (char *)malloc(rlen + 1);
    if (!dirnamep) {
        free(name);
        return -1;
    }
    memcpy(*dirnamep, ret, rlen);
    (*dirnamep)[rlen] = '\0';
    
    free(name);

    return 0;
}

static ss_int_t
ss_check_config()
{
    ss_map_t    *tmp, filemap;
    char        *ok[5]; 
    ss_int_t    i, plen, wlen;

    memset(ok, (char)0, sizeof(ok));
    filemap.next = fileconf;
    tmp = &filemap;
//the fileconf only includes "wwwpath" "listen" "403" "404" "500",they can be identified by the third byte       
    while(tmp->next) {
        tmp = tmp->next;
        switch (tmp->key[2]) {
            case 'w':
                if (access(tmp->value, R_OK | X_OK) == -1) {
                    ss_stderr_log("not access to allow %s or not exists", tmp->value);
                    return -1;
                }
                ok[0] = tmp->value;
                break;
            case 's':
                if (atoi(tmp->value) <= 0) {
                    ss_stderr_log("listen port:%d is not allowed", atoi(tmp->value));
                    return -1;
                }
                ok[1] = tmp->value;
                break;
            case '3':
                if (access(tmp->value, R_OK) == -1) {
                    ss_stderr_log("not access to 403 error file or not exists:%s\n", tmp->value);
                    return -1;
                }
                ok[2] = tmp->value;
                break;
            case '4':
                if (access(tmp->value, R_OK) == -1) {
                    ss_stderr_log("not access to 404 error file or not exists:%s\n", tmp->value);
                    return -1;
                }
                ok[3] = tmp->value;
                break;
            case '0':
                if (access(tmp->value, R_OK) == -1) {
                    ss_stderr_log("not access to 500 error file or not exists:%s\n", tmp->value);
                    return -1;
                }
                ok[4] = tmp->value;
                break;
            default:
                ss_stderr_log("parse config must be error:%c\n", tmp->key[2]);
                return -1;
        }
    }
    //set the wwwpath
    if (ok[0]) {
        wwwpath.data = ok[0];
        wwwpath.len  = strlen(ok[0]);
    } else {
        plen = strlen(workdir);
        wlen = strlen(SS_DEFAULT_WWWPATH);
        
        wwwpath.data = (char *)malloc(plen + wlen + 1);
        if (wwwpath.data == NULL) {
            ss_stderr_log("malloc for wwwpath error\n");
            return -1;
        }
        memcpy(wwwpath.data, workdir, plen);
        memcpy(wwwpath.data + plen, "/"SS_DEFAULT_WWWPATH, wlen + 1);
        wwwpath.len = plen + wlen + 1;
    }
    return 0;
}
