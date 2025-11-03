# Ecosystem

## Project Architecture

To make the app extensible, the implementation uses an entity/behavior driven system. Adding new species and behaviors requires 3 (or 4) steps:
- Create the behavior class
- Create the species traits definition
- Link the behavior to the species
- (Optional) Add the config to the GUI

OpenGL is used for the grid rendering. ImGui is used for the interface. All logic is handled with pure C++. Because the development is done on MacOS, compute shaders are not usable due to the available OpenGL version being too low.

## Subject (French)

On veut simuler la croissance et décroissance de populations animales.

On considérera une zone d'une taille donnée, représentée par des cases. Chaque case peut
recevoir un animal et un végétal (mais pas 2 animaux ni 2 végétaux). Les animaux
peuvent se déplacer d'une case par tour, les végétaux restent immobiles.

Si deux animaux de la même espèce se croisent (sont sur des cases côte à côte)
un nouvel animal de la même espèce apparaîtra sur une case proche.

Tous les végétaux se développeront et feront apparaître sur toutes les cases côte
à côte avec eux un nouveau végétal tous les X tours (mettez un nombre qui permet
aux plantes de subsister).

Il existera des animaux herbivores et carnivores, les carnivores mangent les
herbivores, les herbivores mangent les plantes.

Le but est de rendre un code soigné, en C++, et de respecter les bonnes
pratiques et/ou outils que l’on a vu. Le rendu doit être un exécutable directement
fonctionnel et le code source du projet.

L’affichage peut être fait en console ou en utilisant une bibliothèque graphique
de votre choix (notamment Open Mesh).

Si vous avez du temps et souhaitez aller plus loin vous pouvez tester vos choix
d’architecture de code en essayant d’ajouter les bonus et en équilibrant les valeurs
pour que chaque espèce survive le plus possible !

Bonus :

- [x] Ajouter un genre aux animaux (mâle et femelle)
- [ ] Après une naissance le nouveau né reste proche d’un parent pendant plusieurs tours
- [x] Les animaux peuvent mourir de faim si ils restent trop longtemps sans manger.
- [x] Les animaux qui viennent de se nourrir n’ont plus faim pour quelques tours
- [x] Les animaux recherchent leur nourriture ~ou un partenaire de reproduction~ à
  quelques cases autour d’eux. Ils essaient aussi de fuir pour ne pas être mangés.
