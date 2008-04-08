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
-- Table structure for table `extensions`
--

DROP TABLE IF EXISTS `extensions`;
CREATE TABLE `extensions` (
  `filetype` smallint(5) unsigned NOT NULL,
  `extension` varchar(10) NOT NULL,
  KEY `filetype` (`filetype`),
  CONSTRAINT `extensions_ibfk_1` FOREIGN KEY (`filetype`) REFERENCES `filetypes` (`id`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

--
-- Table structure for table `filenames`
--

DROP TABLE IF EXISTS `filenames`;
CREATE TABLE `filenames` (
  `filetype` smallint(5) unsigned NOT NULL,
  `filename` varchar(30) NOT NULL,
  KEY `filetype` (`filetype`),
  CONSTRAINT `filenames_ibfk_1` FOREIGN KEY (`filetype`) REFERENCES `filetypes` (`id`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

--
-- Table structure for table `filetypes`
--

DROP TABLE IF EXISTS `filetypes`;
CREATE TABLE `filetypes` (
  `id` smallint(5) unsigned NOT NULL auto_increment,
  `name` varchar(20) NOT NULL,
  `subdir` varchar(40) default NULL,
  `text` tinyint(1) NOT NULL,
  PRIMARY KEY  (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

--
-- Table structure for table `metrics`
--

DROP TABLE IF EXISTS `metrics`;
CREATE TABLE `metrics` (
  `repository` tinyint(3) unsigned NOT NULL,
  `filetype` smallint(5) unsigned NOT NULL,
  `revision` mediumint(8) unsigned NOT NULL,
  `count` smallint(5) unsigned NOT NULL,
  `size` int(10) unsigned NOT NULL,
  `lines` mediumint(8) unsigned default NULL,
  KEY `repository_2` (`repository`,`filetype`),
  KEY `repository` (`repository`,`revision`),
  CONSTRAINT `metrics_ibfk_1` FOREIGN KEY (`repository`, `filetype`) REFERENCES `repo_type_assoc` (`repository`, `type`) ON DELETE CASCADE ON UPDATE CASCADE,
  CONSTRAINT `metrics_ibfk_2` FOREIGN KEY (`repository`, `revision`) REFERENCES `revisions` (`repository`, `revision`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

--
-- Table structure for table `repo_type_assoc`
--

DROP TABLE IF EXISTS `repo_type_assoc`;
CREATE TABLE `repo_type_assoc` (
  `repository` tinyint(3) unsigned NOT NULL,
  `type` smallint(5) unsigned NOT NULL,
  KEY `repository` (`repository`,`type`),
  KEY `type` (`type`),
  CONSTRAINT `repo_type_assoc_ibfk_2` FOREIGN KEY (`type`) REFERENCES `filetypes` (`id`) ON DELETE CASCADE ON UPDATE CASCADE,
  CONSTRAINT `repo_type_assoc_ibfk_1` FOREIGN KEY (`repository`) REFERENCES `repository` (`id`) ON DELETE CASCADE ON UPDATE CASCADE
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

-- Dump completed on 2008-04-08 12:02:12
