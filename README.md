### TagIt

TagIt is a file system metadata indexing framework that is integrated into the
file system itself.  Being integrated into the file system, i.e., instead of
staying outside of the file system, allows to provide useful data management
services in addition to the performance benefits.  The idea is presented in the
following papers:

* Hyogi Sim, Awais Khan, Sudharshan S. Vazhkudai, Seung-Hwan Lim, Ali Raza
Butt, and Youngjae Kim, "[An Integrated Indexing and Search Service for
Distributed File Systems](https://doi.org/10.1109/TPDS.2020.2990656),"
IEEE Transactions on Parallel Distributed Systems (TPDS), vol. 31, no. 10, pp.
2375–2391, 2020.

* Hyogi Sim, Youngjae Kim, Sudharshan S. Vazhkudai, Geoffroy R. Vallée,
Seung-Hwan Lim, and Ali R. Butt, "[Tagit: An Integrated Indexing and Search
Service for File Systems](https://doi.org/10.1145/3126908.3126929),"
in Proceedings of the International Conference for High Performance Computing,
Networking, Storage and Analysis (SC), New York, NY, USA, 2017.

### Implementation

TagIt has been implemented using the
[translator framework of GlusterFS](https://glusterdocs-beta.readthedocs.io/en/latest/overview-concepts/translators.html).
TagIt appends two additional translators, one in the
[client-side](https://github.com/sandrain/tagit-gluster/tree/tagit/xlators/features/imess-client/src)
and the other in the
[server-side](https://github.com/sandrain/tagit-gluster/tree/tagit/xlators/features/imess-server/src).
Also, TagIt adds some more entries into the
[meta](https://github.com/sandrain/tagit-gluster/tree/tagit/xlators/meta/src)
translator.

### Orignal README from GlusterFS

For information about contributing to GlusterFS, please follow the below link :
[Contributing to GlusterFS community](http://www.gluster.org/community/documentation/index.php/Main_Page#Contributing_to_the_Gluster_Community)

*GlusterFS does not follow the [GitHub: Fork & pull](https://help.github.com/articles/using-pull-requests/) workflow but use [Gerrit](http://review.gluster.org) for code review.*

The development guidelines are detailed in [Development Workflow.](http://www.gluster.org/community/documentation/index.php/Simplified_dev_workflow)

For more info, please visit http://www.gluster.org/.
