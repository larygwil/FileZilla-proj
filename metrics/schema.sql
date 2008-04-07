-- MySQL dump 10.11
--
-- Host: localhost    Database: code_metrics
-- ------------------------------------------------------
-- Server version	5.0.32-Debian_7etch5-log

/*!40101 SET @OLD_CHARACTER_SET_CLIENT=@@CHARACTER_SET_CLIENT */;
/*!40101 SET @OLD_CHARACTER_SET_RESULTS=@@CHARACTER_SET_RESULTS */;
/*!40101 SET @OLD_COLLATION_CONNECTION=@@COLLATION_CONNECTION */;
/*!40101 SET NAMES utf8 */;
/*!40103 SET @OLD_TIME_ZONE=@@TIME_ZONE */;
/*!40103 SET TIME_ZONE='+00:00' */;
/*!40014 SET @OLD_UNIQUE_CHECKS=@@UNIQUE_CHECKS, UNIQUE_CHECKS=0 */;
/*!40014 SET @OLD_FOREIGN_KEY_CHECKS=@@FOREIGN_KEY_CHECKS, FOREIGN_KEY_CHECKS=0 */;
/*!40101 SET @OLD_SQL_MODE=@@SQL_MODE, SQL_MODE='NO_AUTO_VALUE_ON_ZERO' */;
/*!40111 SET @OLD_SQL_NOTES=@@SQL_NOTES, SQL_NOTES=0 */;

--
-- Table structure for table `metrics`
--

DROP TABLE IF EXISTS `metrics`;
CREATE TABLE `metrics` (
  `repository` tinyint(3) unsigned NOT NULL,
  `revision` mediumint(8) unsigned NOT NULL,
  `loc_header` mediumint(8) unsigned NOT NULL,
  `loc_source` mediumint(8) unsigned NOT NULL,
  `loc_other` mediumint(8) unsigned NOT NULL,
  `size_header` int(10) unsigned NOT NULL,
  `size_source` int(10) unsigned NOT NULL,
  `size_other` int(10) unsigned NOT NULL,
  `size_binary` int(10) unsigned NOT NULL,
  `count_header` smallint(5) unsigned NOT NULL,
  `count_source` smallint(5) unsigned NOT NULL,
  `count_other` smallint(5) unsigned NOT NULL,
  `count_binary` smallint(5) unsigned NOT NULL,
  PRIMARY KEY  (`repository`,`revision`),
  KEY `repository` (`repository`),
  KEY `revision` (`revision`),
  CONSTRAINT `metrics_ibfk_1` FOREIGN KEY (`repository`, `revision`) REFERENCES `revisions` (`repository`, `revision`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

--
-- Table structure for table `repository`
--

DROP TABLE IF EXISTS `repository`;
CREATE TABLE `repository` (
  `id` tinyint(3) unsigned NOT NULL auto_increment,
  `name` varchar(20) NOT NULL COMMENT 'Human readable name',
  `svndir` varchar(15) NOT NULL COMMENT 'SVN working directory',
  PRIMARY KEY  (`id`),
  UNIQUE KEY `svndir` (`svndir`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

--
-- Table structure for table `revisions`
--

DROP TABLE IF EXISTS `revisions`;
CREATE TABLE `revisions` (
  `repository` tinyint(3) unsigned NOT NULL,
  `revision` mediumint(8) unsigned NOT NULL,
  `date` datetime NOT NULL,
  PRIMARY KEY  (`repository`,`revision`),
  KEY `date` (`date`),
  KEY `revision` (`revision`),
  KEY `repository` (`repository`),
  CONSTRAINT `revisions_ibfk_1` FOREIGN KEY (`repository`) REFERENCES `repository` (`id`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=latin1;
/*!40103 SET TIME_ZONE=@OLD_TIME_ZONE */;

/*!40101 SET SQL_MODE=@OLD_SQL_MODE */;
/*!40014 SET FOREIGN_KEY_CHECKS=@OLD_FOREIGN_KEY_CHECKS */;
/*!40014 SET UNIQUE_CHECKS=@OLD_UNIQUE_CHECKS */;
/*!40101 SET CHARACTER_SET_CLIENT=@OLD_CHARACTER_SET_CLIENT */;
/*!40101 SET CHARACTER_SET_RESULTS=@OLD_CHARACTER_SET_RESULTS */;
/*!40101 SET COLLATION_CONNECTION=@OLD_COLLATION_CONNECTION */;
/*!40111 SET SQL_NOTES=@OLD_SQL_NOTES */;

-- Dump completed on 2008-04-07 15:24:03
