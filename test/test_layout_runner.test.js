'use strict';

const assert = require('node:assert/strict');
const path = require('node:path');
const { once } = require('node:events');
const { test } = require('node:test');
const { RadiantLayoutTester } = require('./layout/test_radiant_layout');

const root = path.resolve(__dirname, '..');
const hostRoot = path.join(root, 'test');

test('external layout hosts use their checkout while artifacts stay in Radiant', async () => {
    const tester = new RadiantLayoutTester({
        projectRoot: root, radiantExe: process.execPath, hostWorkingDir: hostRoot
    });
    const child = tester.spawnLayoutProcess(['-e', 'process.stdout.write(process.cwd())']);
    let output = '';
    child.stdout.on('data', chunk => { output += chunk; });
    const [code, signal] = await once(child, 'close');
    assert.equal(code, 0);
    assert.equal(signal, null);
    assert.equal(output, hostRoot);
    assert.equal(tester.outputFile, path.join(root, 'temp/view_tree.json'));
    assert.equal(tester.processRegistry.size, 0);
});

async function categoryResult({ failed = 0, fullSuiteFailures = failed, missing = false } = {}) {
    const tester = new RadiantLayoutTester({
        projectRoot: root, hostWorkingDir: hostRoot, baselineOnly: true
    });
    tester.prepareCategoryPlanInput = async category => {
        if (missing) throw new Error('missing fixture directory');
        return category;
    };
    tester.buildTestPlan = async categories => ({
        categories: new Map(categories.map(category => [category, {}]))
    });
    const original = RadiantLayoutTester.prototype.testCategory;
    const log = console.log;
    const messages = [];
    console.log = message => { messages.push(message); };
    RadiantLayoutTester.prototype.testCategory = async function(category) {
        assert.equal(this.hostWorkingDir, hostRoot);
        this._categorySummaries = { [category]: {
            category, total: failed + 1, successful: 1, failed, skipped: 0,
            baselineRegressions: 0
        } };
        this._fullSuiteFailures = fullSuiteFailures;
        return Array.from({ length: failed + 1 }, () => ({}));
    };
    try {
        await tester.testCategories(['baseline']);
    } finally {
        RadiantLayoutTester.prototype.testCategory = original;
        console.log = log;
    }
    return { tester, messages };
}

test('unscored geometry failures reach the combined layout gate and report', async () => {
    const { tester, messages } = await categoryResult({ failed: 51 });
    assert.equal(tester._fullSuiteFailures, 51);
    assert(messages.includes('LAYOUT_CATEGORY_RESULT|baseline|FAIL|1|51|0'));
});

test('passing layout categories leave the combined gate successful', async () => {
    const { tester, messages } = await categoryResult();
    assert.equal(tester._fullSuiteFailures || 0, 0);
    assert(messages.includes('LAYOUT_CATEGORY_RESULT|baseline|PASS|1|0|0'));
});

test('accepted scored-baseline partials keep their existing classification', async () => {
    const { tester, messages } = await categoryResult({ failed: 51, fullSuiteFailures: 0 });
    assert.equal(tester._fullSuiteFailures || 0, 0);
    assert(messages.includes('LAYOUT_CATEGORY_RESULT|baseline|PASS|1|0|0'));
});

test('a missing layout category fails the combined gate', async () => {
    const { tester, messages } = await categoryResult({ missing: true });
    assert.equal(tester._fullSuiteFailures, 1);
    assert(messages.includes('LAYOUT_CATEGORY_RESULT|baseline|FAIL|0|1|0'));
});
