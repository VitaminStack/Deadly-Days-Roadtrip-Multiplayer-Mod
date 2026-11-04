"""Zusammenfassung der Tunnel-Zeilen fuer den Test auf zwei PCs."""
import unittest

from tools.spike import tunnel_report as report

STATS = ('Tunnel: Verbindung 7 verbunden, zu Steam 10 Pakete/900 B, von Steam 12 Pakete/1500 B, '
         'verworfen {dropped}, Ping {ping} ms, Qualitaet 0.980000')


class TunnelReportTests(unittest.TestCase):
    def test_connected_with_ping_and_route_is_ok(self):
        lines = [
            'Tunnel: eigene SteamID 76561190000000001',
            'Tunnel: Client verbindet ueber P2P, Verbindung 7, Spiel auf UDP 7778',
            'Tunnel: Verbindung 7 wartet, zu Steam 0 Pakete/0 B, von Steam 0 Pakete/0 B, verworfen 0, Ping -1 ms, Qualitaet -1.000000',
            'Tunnel: Route Transport: SDR relay via fra#12 (203.0.113.9:27015)',
            'Tunnel: Route Transport: SDR relay via fra#12 (203.0.113.9:27015)',
            STATS.format(dropped=0, ping=31),
            STATS.format(dropped=2, ping=45),
            STATS.format(dropped=2, ping=-1),
            'anderes Log ohne Tunnel',
        ]
        summary = report.summarize(lines)
        self.assertTrue(summary['ok'])
        self.assertEqual(summary['pings'], [31, 45])
        self.assertEqual(summary['routes'], ['Transport: SDR relay via fra#12 (<ip>:27015)'])
        text = report.render(summary, 'Client')
        self.assertIn('Ping: min 31 ms, Median 38 ms, max 45 ms', text)
        self.assertIn('verworfen 2, Qualitaet 0.98', text)
        self.assertIn('Ping und Route belegt', text)

    def test_rejected_connection_is_not_ok(self):
        summary = report.summarize(['Tunnel: Steam hat die Verbindung abgelehnt (Handle 0)'])
        self.assertFalse(summary['ok'])
        self.assertEqual(summary['problems'], ['Tunnel: Steam hat die Verbindung abgelehnt (Handle 0)'])
        self.assertIn('NICHT belegt', report.render(summary, 'Client'))

    def test_missing_route_is_not_ok(self):
        self.assertFalse(report.summarize([STATS.format(dropped=0, ping=20)])['ok'])

    def test_mask_hides_accounts_and_addresses_but_keeps_loopback(self):
        line = 'peer 76561198000000000 [U:1:12345] steamid:76561198000000000 at 198.51.100.7, local 127.0.0.1'
        self.assertEqual(report.mask(line), 'peer <steamid> <steamid> <steamid> at <ip>, local 127.0.0.1')

    def test_end_lines_are_kept_masked(self):
        summary = report.summarize(['Tunnel: Verbindung beendet, Grund 4001 peer 198.51.100.7 timed out'])
        self.assertEqual(summary['ends'], ['Tunnel: Verbindung beendet, Grund 4001 peer <ip> timed out'])


if __name__ == '__main__':
    unittest.main()
