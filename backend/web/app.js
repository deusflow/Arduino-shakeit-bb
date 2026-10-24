(() => {
    let countries = [];
    let selectedCountry = null;
    let selectedRegion = 'All';
    let searchQuery = '';
    let isFetching = false;

    const heroLabel = document.getElementById('heroLabel');
    const heroContent = document.getElementById('heroContent');
    const savedList = document.getElementById('savedList');
    const regionChips = document.getElementById('regionChips');
    const searchInput = document.getElementById('searchInput');
    const refreshBtn = document.getElementById('refreshBtn');

    function escapeHtml(str) {
        if (!str) return '';
        return String(str)
            .replace(/&/g, '&amp;')
            .replace(/</g, '&lt;')
            .replace(/>/g, '&gt;')
            .replace(/"/g, '&quot;')
            .replace(/'/g, '&#39;');
    }

    function formatNumber(num) {
        if (num === null || num === undefined) return '0';
        return Number(num).toLocaleString('en-US');
    }

    function formatDate(isoStr, shortOnly = false) {
        if (!isoStr) return '';
        const d = new Date(isoStr);
        if (isNaN(d.getTime())) return isoStr;

        const pad = (n) => String(n).padStart(2, '0');
        const hours = pad(d.getUTCHours());
        const mins = pad(d.getUTCMinutes());
        const secs = pad(d.getUTCSeconds());

        if (shortOnly) {
            return `${hours}:${mins}:${secs}`;
        }

        const year = d.getUTCFullYear();
        const month = pad(d.getUTCMonth() + 1);
        const day = pad(d.getUTCDate());
        return `${year}-${month}-${day} ${hours}:${mins}:${secs}`;
    }

    async function fetchCountries() {
        if (isFetching) return;
        isFetching = true;
        refreshBtn.textContent = 'Refreshing...';
        refreshBtn.disabled = true;

        try {
            const resp = await fetch('/api/countries');
            if (!resp.ok) {
                throw new Error(`HTTP ${resp.status}`);
            }
            const data = await resp.json();
            countries = Array.isArray(data) ? data : [];

            if (selectedCountry && !countries.some(c => c.id === selectedCountry.id)) {
                selectedCountry = null;
            }

            renderRegions();
            renderHero();
            renderList();
        } catch (err) {
            if (countries.length === 0) {
                heroContent.innerHTML = `<div class="state-message error">Cannot connect to backend (${escapeHtml(err.message)})</div>`;
            }
        } finally {
            isFetching = false;
            refreshBtn.textContent = 'Refresh';
            refreshBtn.disabled = false;
        }
    }

    function renderRegions() {
        const distinct = ['All'];
        countries.forEach(c => {
            if (c.region && !distinct.includes(c.region)) {
                distinct.push(c.region);
            }
        });

        const currentButtons = Array.from(regionChips.querySelectorAll('.region-btn')).map(b => b.dataset.region);
        const isSame = distinct.length === currentButtons.length && distinct.every((r, i) => r === currentButtons[i]);
        if (isSame) {
            regionChips.querySelectorAll('.region-btn').forEach(btn => {
                btn.classList.toggle('active', btn.dataset.region === selectedRegion);
            });
            return;
        }

        regionChips.innerHTML = '';
        distinct.forEach(region => {
            const btn = document.createElement('button');
            btn.className = `region-btn ${region === selectedRegion ? 'active' : ''}`;
            btn.dataset.region = region;
            btn.textContent = region;
            btn.addEventListener('click', () => {
                selectedRegion = region;
                renderRegions();
                renderList();
            });
            regionChips.appendChild(btn);
        });
    }

    function renderHero() {
        const country = selectedCountry || (countries.length > 0 ? countries[0] : null);
        heroLabel.textContent = selectedCountry ? 'Selected Country' : 'Latest Recorded';

        if (!country) {
            heroContent.innerHTML = `
                <div class="state-message">
                    No countries recorded yet.<br>
                    Shake your Arduino Oplà and press Touch 0 to save your first country.
                </div>
            `;
            return;
        }

        heroContent.innerHTML = `
            <h2 class="country-display-name">${escapeHtml(country.choice)}</h2>
            <div class="facts-table">
                <span class="fact-title">Capital</span>
                <span class="fact-value">${escapeHtml(country.capital)}</span>

                <span class="fact-title">Region</span>
                <span class="fact-value">${escapeHtml(country.region)}</span>

                <span class="fact-title">ISO Code</span>
                <span class="fact-value">${escapeHtml(country.cca3)}</span>

                <span class="fact-title">Population</span>
                <span class="fact-value tabular">${formatNumber(country.population)}</span>

                <span class="fact-title">Data Source</span>
                <span class="fact-value">${escapeHtml(country.apiName)}</span>
            </div>
            <p class="save-timestamp">
                Saved at ${formatDate(country.savedAt)} UTC &middot; Record #${country.id}
            </p>
        `;
    }

    function renderList() {
        const query = searchQuery.trim().toLowerCase();
        const filtered = countries.filter(c => {
            const matchesRegion = selectedRegion === 'All' || (c.region && c.region.toLowerCase() === selectedRegion.toLowerCase());
            const matchesSearch = !query ||
                (c.choice && c.choice.toLowerCase().includes(query)) ||
                (c.capital && c.capital.toLowerCase().includes(query)) ||
                (c.cca3 && c.cca3.toLowerCase().includes(query));
            return matchesRegion && matchesSearch;
        });

        if (filtered.length === 0) {
            if (countries.length === 0) {
                savedList.innerHTML = '';
            } else {
                savedList.innerHTML = '<div class="state-message">No records match the current filters.</div>';
            }
            return;
        }

        savedList.innerHTML = filtered.map(c => {
            const isSelected = selectedCountry && selectedCountry.id === c.id;
            return `
                <div class="country-item ${isSelected ? 'selected' : ''}" data-id="${c.id}">
                    <div>
                        <p class="country-item-name">${escapeHtml(c.choice)}</p>
                        <span class="country-item-meta">${escapeHtml(c.capital)}, ${escapeHtml(c.region)}</span>
                    </div>
                    <span class="country-item-meta tabular">${formatDate(c.savedAt, true)}</span>
                </div>
            `;
        }).join('');

        savedList.querySelectorAll('.country-item').forEach(item => {
            item.addEventListener('click', () => {
                const id = Number(item.dataset.id);
                selectedCountry = countries.find(c => c.id === id) || null;
                renderHero();
                renderList();
            });
        });
    }

    searchInput.addEventListener('input', (e) => {
        searchQuery = e.target.value;
        renderList();
    });

    refreshBtn.addEventListener('click', () => {
        fetchCountries();
    });

    fetchCountries();
    setInterval(fetchCountries, 3000);
})();
