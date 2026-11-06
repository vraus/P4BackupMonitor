# P4 Backup Monitor

Suivi, depuis un poste administrateur, de la sauvegarde Perforce mise en place par le kit
PerforceBackup (sauvegarde du soir, test quotidien, test complet hebdomadaire).

Version 0.1 : **monitoring en lecture seule**. Rien n'est modifié sur le serveur ni sur le NAS.

## Ce que l'application affiche

- Bandeau d'état général : vert, orange ou rouge, avec le point le plus grave.
- Trois cartes : âge de la dernière sauvegarde réussie, espace libre du serveur, espace libre du NAS.
- Onglet « Points à traiter » : chaque problème détecté, avec son code du guide de dépannage.
  Double-clic : ouvre la fiche dans le guide HTML.
- Onglet « Tâches planifiées » : dernière exécution, résultat traduit, prochaine exécution, compte.
- Onglet « Journaux » : journaux des 8 derniers jours, lignes [ERROR] / [WARN] avec leur code,
  texte complet coloré.
- Actualisation manuelle ou automatique (5, 15, 30 ou 60 min).

## Architecture

```
P4BackupMonitor.exe  (Qt 6, C++)
   │  lance, attend, lit le JSON
   ▼
collector\Get-BackupStatus.ps1  (Windows PowerShell 5.1, lecture seule)
   ├─ \\SERVEUR\C$\PerforceBackup\config.psd1 et \logs   (partage administratif)
   ├─ tâches planifiées, disques                          (CIM : WinRM, sinon DCOM)
   └─ <NasRoot>\status\last-backup.json, espace libre     (partage du NAS)
```

Les accès Windows à distance (tâches planifiées, WMI, partages) sont faits par PowerShell, qui les
gère nativement. L'application Qt ne fait que l'affichage et l'interprétation.

Le collecteur peut être lancé seul pour diagnostiquer un problème de collecte :

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\collector\Get-BackupStatus.ps1 -Server NUC-LAB
```

Chaque source est indépendante : une source inaccessible apparaît dans `problems` sans bloquer les
autres.

## Prérequis

**Poste client**
- Windows 10 ou 11, PowerShell 5.1 (inclus).
- Session ouverte avec un compte **administrateur du serveur** (compte du domaine), qui a aussi
  la lecture sur le dossier de sauvegarde du NAS.

**Serveur Perforce**
- Partage administratif `C$` joignable depuis le poste (cas par défaut dans un domaine).
- L'un des deux accès suivants pour les tâches et les disques :
  - WinRM, le plus simple : `Enable-PSRemoting -Force` sur le serveur (PowerShell administrateur) ;
  - ou DCOM : règles de pare-feu « Infrastructure de gestion Windows (WMI) » activées.

**Labo hors domaine (groupe de travail)**
- Utiliser un compte local administrateur du même nom et du même mot de passe sur les deux machines,
  ou ouvrir la session réseau avec `net use \\NUC-LAB\C$ /user:NUC-LAB\admin`.
- Sur le serveur, autoriser les comptes locaux à distance (UAC filtre sinon leurs droits admin) :
  `reg add HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\Policies\System /v LocalAccountTokenFilterPolicy /t REG_DWORD /d 1 /f`
- WinRM hors domaine : sur le poste client, `Set-Item WSMan:\localhost\Client\TrustedHosts NUC-LAB`.

## Règles d'état

| Élément | Orange | Rouge |
| --- | --- | --- |
| Dernière sauvegarde | | plus vieille que `MaxCheckpointAgeHours` (26 h) |
| Disque serveur, NAS | moins de 20 % libres | moins de 10 % libres |
| Tâche planifiée | désactivée, jamais exécutée | dernier résultat en échec ; pas exécutée depuis 26 h (8 j pour le test complet) |
| Dernier journal d'un type | [WARN], ou sans bilan depuis plus de 6 h | [ERROR] |
| Collecte | | source inaccessible (config, CIM, NAS, statut, journaux) |

Les seuils sont dans `src/Health.cpp`.

## Compilation (Windows)

1. Installer Qt 6.5 ou plus récent (composant MSVC 2022 64 bits) et Visual Studio 2022 (C++).
2. Ouvrir `CMakeLists.txt` dans Qt Creator, ou en ligne de commande :
   ```
   cmake -S . -B build -DCMAKE_PREFIX_PATH=C:\Qt\6.8.0\msvc2022_64
   cmake --build build --config Release
   ```
3. Le collecteur est copié automatiquement dans `build\Release\collector\`.
4. Pour distribuer : `windeployqt build\Release\P4BackupMonitor.exe`, puis copier le dossier entier.

## Développement sans serveur

```
P4BackupMonitor --demo samples\demo-nuk.json
P4BackupMonitor --demo samples\demo-nuk.json --tab 2 --screenshot capture.png
```

Menu Fichier, « Ouvrir un état enregistré » : relit un JSON produit par le collecteur
(`-OutFile etat.json`), pratique pour reproduire un cas sans le serveur.

## Suite prévue

- v0.2 : édition de `config.psd1` avec bouton « Inventaire » (Get-SiteInventory.ps1) et vérification
  des champs.
- v0.3 : mise en service (Enable-Backup) à distance. Contrainte : une session distante ne peut pas
  rebondir vers le NAS (double saut Kerberos). Piste : déposer les scripts sur le serveur puis lancer
  la mise en service par une tâche planifiée ponctuelle sous le compte de sauvegarde.
- Notification Windows (zone de notification) quand l'état passe au rouge.
