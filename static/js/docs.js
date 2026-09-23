(() => {
    const RESET_DELAY_MS = 1500;

    document.querySelectorAll('.code-block').forEach((block) => {
        const button = block.querySelector('.copy-btn');
        const code = block.querySelector('pre code');
        if (!button || !code) return;

        const defaultLabel = button.textContent;
        let resetTimer = null;

        button.addEventListener('click', async () => {
            try {
                await navigator.clipboard.writeText(code.textContent);
                button.textContent = 'Copied';
                button.classList.add('copied');
            } catch (err) {
                button.textContent = 'Failed';
            }

            clearTimeout(resetTimer);
            resetTimer = setTimeout(() => {
                button.textContent = defaultLabel;
                button.classList.remove('copied');
            }, RESET_DELAY_MS);
        });
    });
})();
