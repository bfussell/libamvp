/*
 * Copyright (c) 2023, Cisco Systems, Inc.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://github.com/cisco/libamvp/LICENSE
 */

/*
 * This module is not part of libamvp.  Rather, it's a simple app that
 * demonstrates how to use libamvp. Software that use libamvp
 * will need to implement a similar module.
 *
 * It will default to 127.0.0.1 port 443 if no arguments are given.
 */
#include <stdio.h>
#include <openssl/rsa.h>
#include <openssl/bn.h>

#include "app_lcl.h"

#if OPENSSL_VERSION_NUMBER >= 0x30000000L
#include <openssl/provider.h>
#include <openssl/evp.h>
#ifdef _WIN32
#include <Windows.h>
#else
#include <unistd.h>
#endif
#endif

#include "safe_mem_lib.h"
#include "safe_str_lib.h"

static int enable_hash(AMVP_CTX *ctx);

const char *server;
int port;
const char *ca_chain_file;
char *cert_file;
char *key_file;
const char *path_segment;
const char *api_context;
char value[JSON_STRING_LENGTH] = "same";

#define CHECK_ENABLE_CAP_RV(rv) \
    if (rv != AMVP_SUCCESS) { \
        printf("Failed to register capability with libamvp (rv=%d: %s)\n", rv, amvp_lookup_error_string(rv)); \
        goto end; \
    }

/*
 * Read the operational parameters from the various environment
 * variables.
 */
static void setup_session_parameters(void) {
    char *tmp;

    server = getenv("AMV_SERVER");
    if (!server) {
         server = DEFAULT_SERVER;
     }

    tmp = getenv("AMV_PORT");
    if (tmp) port = atoi(tmp);
    if (!port) port = DEFAULT_PORT;

    path_segment = getenv("AMV_URI_PREFIX");
    if (!path_segment) path_segment = DEFAULT_URI_PREFIX;

    api_context = getenv("AMV_API_CONTEXT");
    if (!api_context) api_context = "";

    ca_chain_file = getenv("AMV_CA_FILE");
    cert_file = getenv("AMV_CERT_FILE");
    key_file = getenv("AMV_KEY_FILE");

    printf("Using the following parameters:\n\n");
    printf("    AMV_SERVER:     %s\n", server);
    printf("    AMV_PORT:       %d\n", port);
    printf("    AMV_URI_PREFIX: %s\n", path_segment);
    if (ca_chain_file) printf("    AMV_CA_FILE:    %s\n", ca_chain_file);
    if (cert_file) printf("    AMV_CERT_FILE:  %s\n", cert_file);
    if (key_file) printf("    AMV_KEY_FILE:   %s\n", key_file);
    printf("\n");
}

/* libamvp calls this function for status updates, debugs, warnings, and errors. */
static AMVP_RESULT progress(char *msg, AMVP_LOG_LVL level) {

    printf("[AMVP]");

    switch (level) {
    case AMVP_LOG_LVL_ERR:
        printf(ANSI_COLOR_RED "[ERROR]" ANSI_COLOR_RESET);
        break;
    case AMVP_LOG_LVL_WARN:
        printf(ANSI_COLOR_YELLOW "[WARNING]" ANSI_COLOR_RESET);
        break;
    case AMVP_LOG_LVL_STATUS:
    case AMVP_LOG_LVL_INFO:
    case AMVP_LOG_LVL_VERBOSE:
    case AMVP_LOG_LVL_DEBUG:
    case AMVP_LOG_LVL_NONE:
    case AMVP_LOG_LVL_MAX:
    default:
        break;
    }

    printf(": %s\n", msg);

    return AMVP_SUCCESS;
}

static void app_cleanup(AMVP_CTX *ctx) {
    // Routines for libamvp
    amvp_cleanup(ctx);
}


int main(int argc, char **argv) {
    AMVP_RESULT rv = AMVP_SUCCESS;
    AMVP_CTX *ctx = NULL;
    APP_CONFIG cfg;
    int diff = 0;

    memset_s(&cfg, sizeof(APP_CONFIG), 0, sizeof(APP_CONFIG));
    if (ingest_cli(&cfg, argc, argv)) {
        return 1;
    }

#if OPENSSL_VERSION_NUMBER >= 0x30000000L
    if (!cfg.disable_fips) {
        /* sets the property "fips=yes" to be included implicitly in cipher fetches */
        EVP_default_properties_enable_fips(NULL, 1);
        if (!EVP_default_properties_is_fips_enabled(NULL)) {
            printf("Error setting FIPS property at startup\n\n");
            return 1;
        }
        /* Run a quick sanity check to determine that the FIPS provider is functioning properly */
        rv = fips_sanity_check();
        if (rv != AMVP_SUCCESS) {
            printf("Error occured when testing FIPS at startup (rv = %d). Please verify the FIPS provider is\n", rv);
            printf("properly installed and configured. Exiting...\n\n");
            return 1;
        }
    } else {
        printf("***********************************************************************************\n");
        printf("* WARNING: You have chosen to not fetch the FIPS provider for this run. Any tests *\n");
        printf("* created or performed during this run MUST NOT have any validation requested     *\n");
        printf("* on it unless the FIPS provider is exclusively loaded or enabled by default in   *\n");
        printf("* your configuration. Proceed at your own risk. Continuing in 5 seconds...        *\n");
        printf("***********************************************************************************\n");
        printf("\n");
        #ifdef _WIN32
            Sleep(5 * 1000);
        #else
            sleep(5);
        #endif
    }
#endif

     setup_session_parameters();

    /*
     * We begin the libamvp usage flow here.
     * First, we create a test session context.
     */
    rv = amvp_create_test_session(&ctx, &progress, cfg.level);
    if (rv != AMVP_SUCCESS) {
        printf("Failed to create AMVP context: %s\n", amvp_lookup_error_string(rv));
        goto end;
    }

    /* Next we specify the AMVP server address */
    rv = amvp_set_server(ctx, server, port);
    if (rv != AMVP_SUCCESS) {
        printf("Failed to set server/port\n");
        goto end;
    }

    /* Set the api context prefix if needed */
    rv = amvp_set_api_context(ctx, api_context);
    if (rv != AMVP_SUCCESS) {
        printf("Failed to set URI prefix\n");
        goto end;
    }

    /* Set the path segment prefix if needed */
    rv = amvp_set_path_segment(ctx, path_segment);
    if (rv != AMVP_SUCCESS) {
        printf("Failed to set URI prefix\n");
        goto end;
    }

    if (ca_chain_file) {
        /*
         * Next we provide the CA certs to be used by libamvp
         * to verify the AMVP TLS certificate.
         */
        rv = amvp_set_cacerts(ctx, ca_chain_file);
        if (rv != AMVP_SUCCESS) {
            printf("Failed to set CA certs\n");
            goto end;
        }
    }

    if (cert_file && key_file) {
        /*
         * Specify the certificate and private key the client should used
         * for TLS client auth.
         */
        rv = amvp_set_certkey(ctx, cert_file, key_file);
        if (rv != AMVP_SUCCESS) {
            printf("Failed to set TLS cert/key\n");
            goto end;
        }
    }

    /*
     * Setup the Two-factor authentication
     * This may or may not be turned on...
     */
    if (app_setup_two_factor_auth(ctx)) {
        goto end;
    }

    if (cfg.sample) {
        amvp_mark_as_sample(ctx);
    }

    if (cfg.get) {
        rv = amvp_mark_as_get_only(ctx, cfg.get_string);
        if (rv != AMVP_SUCCESS) {
            printf("Failed to mark as get only.\n");
            goto end;
        } else if (cfg.save_to) {
            rv = amvp_set_get_save_file(ctx, cfg.save_file);
            if (rv != AMVP_SUCCESS) {
                printf("Failed to set save file for get request, continuing anyway...\n");
            }
        }
    }

    if (cfg.post) {
        amvp_mark_as_post_only(ctx, cfg.post_filename);
    }

    if (cfg.delete) {
        amvp_mark_as_delete_only(ctx, cfg.delete_url);
    }

    if (cfg.vector_req && !cfg.vector_rsp) {
        amvp_mark_as_request_only(ctx, cfg.vector_req_file);
    }

    if (!cfg.vector_req && cfg.vector_rsp) {
        printf("Offline vector processing requires both options, --vector_req and --vector_rsp\n");
        goto end;
    }

    if (cfg.manual_reg) {
        /*
         * Using a JSON to register allows us to skip the
         * "amvp_enable_*" API calls... could reduce the
         * size of this file if you choose to use this capability.
         */
        rv = amvp_set_json_filename(ctx, cfg.reg_file);
        if (rv != AMVP_SUCCESS) {
            printf("Failed to set json file within AMVP ctx (rv=%d)\n", rv);
            goto end;
        }
    } else {
        if (cfg.hash) { if (enable_hash(ctx)) goto end; }
    }

    if (cfg.get_cost) {
        diff = amvp_get_vector_set_count(ctx);
        if (diff < 0) {
            printf("Unable to get expected vector set count with given test session context.\n\n");
        } else {
            printf("The given test session context is expected to generate %d vector sets.\n\n", diff);
        }
        goto end;
    }

    if (cfg.get_reg) {
        char *reg = NULL;
        reg = amvp_get_current_registration(ctx, NULL);
        if (!reg) {
            printf("Error occured while getting current registration.\n");
            goto end;
        }
        if (cfg.save_to) {
            if (save_string_to_file((const char *)reg, (const char *)&cfg.save_file)) {
                printf("Error occured while saving registration to file. Exiting...\n");
            } else {
                printf("Succesfully saved registration to given file. Exiting...\n");
            }
        } else {
            printf("%s\n", reg);
            printf("Completed output of current registration. Exiting...\n");
        }
        if (reg) free(reg);
        goto end;
    }

    if (cfg.kat) {
       rv = amvp_load_kat_filename(ctx, cfg.kat_file);
       goto end;
    }

    if (cfg.vector_req && cfg.vector_rsp) {
       rv = amvp_run_vectors_from_file(ctx, cfg.vector_req_file, cfg.vector_rsp_file);
       goto end;
    }

    strncmp_s(DEFAULT_SERVER, DEFAULT_SERVER_LEN, server, DEFAULT_SERVER_LEN, &diff);
    if (!diff) {
         printf("Warning: No server set, using default. Please define AMV_SERVER in your environment.\n");
         printf("Run amvp_app --help for more information on this and other environment variables.\n\n");
    }

    if (cfg.fips_validation) {
        unsigned int module_id = 1, oe_id = 1;

        /* Provide the metadata needed for a FIPS validation. */
        rv = amvp_oe_ingest_metadata(ctx, cfg.validation_metadata_file);
        if (rv != AMVP_SUCCESS) {
            printf("Failed to read validation_metadata_file\n");
            goto end;
        }

        /*
         * Tell the library which Module and Operating Environment to use
         * when doing the FIPS validation.
         */
        rv = amvp_oe_set_fips_validation_metadata(ctx, module_id, oe_id);
        if (rv != AMVP_SUCCESS) {
            printf("Failed to set metadata for FIPS validation\n");
            goto end;
        }
    }

    if (cfg.vector_upload) {
       rv = amvp_upload_vectors_from_file(ctx, cfg.vector_upload_file, cfg.fips_validation);
       goto end;
    }

    /* PUT without algorithms submits put_filename for validation using save JWT and testSession ID */
    if (cfg.empty_alg && cfg.put) {
         rv = amvp_put_data_from_file(ctx, cfg.put_filename);
         goto end;
    }
    /* PUT with alg testing will submit put_filename with module/oe information */
    if (!cfg.empty_alg && cfg.put) {
        amvp_mark_as_put_after_test(ctx, cfg.put_filename);
    }
    
    if (cfg.get_results) {
        rv = amvp_get_results_from_server(ctx, cfg.session_file);
        goto end;
    }
    
    if (cfg.resume_session) {
        rv = amvp_resume_test_session(ctx, cfg.session_file, cfg.fips_validation);
        goto end;
    }

    if (cfg.cancel_session) {
        if (cfg.save_to) {
            rv = amvp_cancel_test_session(ctx, cfg.session_file, cfg.save_file);
        } else {
            rv = amvp_cancel_test_session(ctx, cfg.session_file, NULL);
        }
        goto end;
    }

    if (cfg.get_expected) {
        if (cfg.save_to) {
            rv = amvp_get_expected_results(ctx, cfg.session_file, cfg.save_file);
        } else {
            rv = amvp_get_expected_results(ctx, cfg.session_file, NULL);
        }
        goto end;
    }

    if (cfg.post_resources) {
        rv = amvp_mark_as_post_resources(ctx, cfg.post_resources_filename);
    }

    if (cfg.mod_cert_req) {
        rv = amvp_mark_as_cert_req(ctx, cfg.mod_cert_req_file);
    }

    /*
     * Run the test session.
     * Perform a FIPS validation on this test session if specified.
     */
    amvp_run(ctx, cfg.fips_validation);

end:
    /*
     * Free all memory associated with
     * both the application and libamvp.
     */
    app_cleanup(ctx);

    return rv;
}

static int enable_hash(AMVP_CTX *ctx) {
    AMVP_RESULT rv = AMVP_SUCCESS;

    rv = amvp_cap_hash_enable(ctx, AMVP_HASH_SHA256, &app_sha_handler);
    CHECK_ENABLE_CAP_RV(rv);
    rv = amvp_cap_hash_set_domain(ctx, AMVP_HASH_SHA256, AMVP_HASH_MESSAGE_LEN,
                                  0, 65536, 8);
    CHECK_ENABLE_CAP_RV(rv);

end:
    return rv;
}
