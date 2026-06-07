import child_process from 'node:child_process';
import process from 'node:process';
import stream from 'node:stream';
import fs from 'node:fs';
import zlib from 'node:zlib';
import tar from 'tar-stream';
import core from '@actions/core'

const container = core.getInput('container', {required: true});
const error_log_paths = JSON.parse(core.getInput('error-log-paths', {required: true}));
const log_tarball_prefix = core.getInput('log-tarball-prefix', {required: true});
const tests_label = core.getInput('tests-label', {required: true});
const test_timeout = core.getInput('test-timeout', {required: true});

const repo_name = process.env.GITHUB_REPOSITORY.split('/')[1];

try {
   // Defensively remove any leftover container from a prior/interrupted run. On a
   // persistent self-hosted runner the Docker daemon survives between jobs, and the
   // per-test containers below use fixed names with no --rm, so an orphan would block
   // name reuse with "container name already in use". (ENF's ephemeral runners get a
   // fresh daemon each job and never hit this.)
   child_process.spawnSync("docker", ["rm", "-f", "base"], {stdio:"ignore"});
   if(child_process.spawnSync("docker", ["run", "--name", "base", "-v", `${process.cwd()}/build.tar.zst:/build.tar.zst`, "--workdir", `/__w/${repo_name}/${repo_name}`, container, "sh", "-c", "zstdcat /build.tar.zst | tar x"], {stdio:"inherit"}).status)
      throw new Error("Failed to create base container");
   if(child_process.spawnSync("docker", ["commit", "base", "baseimage"], {stdio:"inherit"}).status)
      throw new Error("Failed to create base image");
   if(child_process.spawnSync("docker", ["rm", "base"], {stdio:"inherit"}).status)
      throw new Error("Failed to remove base container");

   const test_query_result = child_process.spawnSync("docker", ["run", "--rm", "baseimage", "bash", "-e", "-o", "pipefail", "-c", `cd build; ctest -L '${tests_label}' --show-only=json-v1`]);
   if(test_query_result.status)
      throw new Error("Failed to discover tests with label")
   const tests = JSON.parse(test_query_result.stdout).tests;

   // Run the test containers with BOUNDED concurrency. The nonparallelizable_tests and
   // long_running_tests suites are heavy multi-nodeos integration tests; launching one
   // container per test all at once (the original behaviour) starves CPU/RAM/ports on a
   // modest self-hosted runner and makes them fail en masse. Cap how many run at a time
   // (override with the CTEST_CONTAINER_CONCURRENCY env var; default 4). results[i] stays
   // aligned with tests[i] so the failure-log extraction below is unchanged.
   const parsed_concurrency = parseInt(process.env.CTEST_CONTAINER_CONCURRENCY, 10);
   const max_concurrency = Number.isNaN(parsed_concurrency) ? 4 : Math.max(1, parsed_concurrency);
   console.log(`Running ${tests.length} '${tests_label}' test(s), up to ${max_concurrency} container(s) at a time`);

   const results = new Array(tests.length);
   let next_test = 0;
   async function run_worker() {
      while(next_test < tests.length) {
         const i = next_test++;
         const t = tests[i];
         // Clear any orphaned container of this name before reusing it (see note above).
         child_process.spawnSync("docker", ["rm", "-f", t.name], {stdio:"ignore"});
         results[i] = await new Promise(resolve => {
            child_process.spawn("docker", ["run", "--security-opt", "seccomp=unconfined", "-e", "GITHUB_ACTIONS=True", "--name", t.name, "--init", "baseimage", "bash", "-c", `cd build; ctest --output-on-failure -R '^${t.name}$' --timeout ${test_timeout}`], {stdio:"inherit"})
               .on('close', code => resolve(code))
               // If docker can't even be spawned, 'close' may never fire — resolve as a
               // failure so the pool doesn't hang and the test is reported as failed.
               .on('error', err => { console.error(`Failed to spawn docker for ${t.name}: ${err.message}`); resolve(1); });
         });
      }
   }
   await Promise.all(Array.from({length: Math.min(max_concurrency, tests.length)}, run_worker));

   for(let i = 0; i < results.length; ++i) {
      if(results[i] === 0)
         continue;

      //failing test
      core.setFailed("Some tests failed");

      let extractor = tar.extract();
      let packer = tar.pack();

      extractor.on('entry', (header, stream, next) => {
         if(!header.name.startsWith(`__w/${repo_name}/${repo_name}/build`)) {
            stream.on('end', () => next());
            stream.resume();
            return;
         }

         header.name = header.name.substring(`__w/${repo_name}/${repo_name}/`.length);
         if(header.name !== "build/" && error_log_paths.filter(p => header.name.startsWith(p)).length === 0) {
            stream.on('end', () => next());
            stream.resume();
            return;
         }

         stream.pipe(packer.entry(header, next));
      }).on('finish', () => {packer.finalize()});

      child_process.spawn("docker", ["export", tests[i].name]).stdout.pipe(extractor);
      stream.promises.pipeline(packer, zlib.createGzip(), fs.createWriteStream(`${log_tarball_prefix}-${tests[i].name}-logs.tar.gz`));
   }
} catch(e) {
   core.setFailed(`Uncaught exception ${e.message}`);
}
